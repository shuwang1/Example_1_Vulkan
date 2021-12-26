#ifdef __cplusplus
extern "C" {
#endif


#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h> 
#include "vulkan/vulkan.h"

#ifdef NDEBUG
	const VkBool32 enableValidationLayers = 0;
#else
	const VkBool32 enableValidationLayers = 1;
#endif

typedef struct {
	VkInstance instance;//a connection between the application and the Vulkan library 
	VkPhysicalDevice physicalDevice;//a handle for the graphics card used in the application
	VkPhysicalDeviceProperties physicalDeviceProperties;//bastic device properties
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;//bastic memory properties of the device
	VkDevice device;//a logical device, interacting with physical device
	VkDebugUtilsMessengerEXT debugMessenger;//extension for debugging
	uint32_t queueFamilyIndex;//if multiple queues are available, specify the used one
	VkQueue queue;//a place, where all operations are submitted
	VkCommandPool commandPool;//an opaque objects that command buffer memory is allocated from
	VkFence fence;//a fence used to synchronize dispatches
	uint32_t device_id;//an id of a device, reported by Vulkan device list
} VkGPU;//an example structure containing Vulkan primitives

typedef struct {
	uint32_t localSize[3];
	uint32_t inputStride[3];
} VkAppSpecializationConstantsLayout;//an example structure on how to set constants in the shader after first compilation but before final shader module creation

typedef struct {
	uint32_t pushID;//an example structure on how to pass small amount of data to the shader right before dispatch
} VkAppPushConstantsLayout;

typedef struct {
	//system size for transposition
	uint32_t size[3];
	//how much memory is coalesced (in bytes) - 32 for Nvidia, 64 for Intel, 64 for AMD. Maximum value: 128
	uint32_t coalescedMemory;
	VkAppSpecializationConstantsLayout specializationConstants;
	VkAppPushConstantsLayout pushConstants;
	//bridging information, that allows shaders to freely access resources like buffers and images
	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;
	//pipeline used for graphics applications, we only use compute part of it in this example
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	//input buffer
	VkDeviceSize inputBufferSize;//the size of buffer (in bytes)
	VkBuffer* inputBuffer;//pointer to the buffer object
	VkDeviceMemory* inputBufferDeviceMemory;//pointer to the memory object, corresponding to the buffer

	//output buffer
	VkDeviceSize outputBufferSize;
	VkBuffer* outputBuffer;
	VkDeviceMemory* outputBufferDeviceMemory;
} VkApplication;//application specific data


uint32_t* VkFFTReadShader(uint32_t* length, const char* filename) {
	//function that reads shader's SPIR - V bytecode
	FILE* fp = fopen(filename, "rb");
	if (fp == NULL) {
		printf("Could not find or open file: %s\n", filename);
	}

	// get file size.
	fseek(fp, 0, SEEK_END);
	long filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	long filesizepadded = ((long)ceil(filesize / 4.0)) * 4;

	char* str = (char*)malloc(sizeof(char) * filesizepadded);
	fread(str, filesize, sizeof(char), fp);
	fclose(fp);

	for (long i = filesize; i < filesizepadded; i++) {
		str[i] = 0;
	}

	length[0] = filesizepadded;
	return (uint32_t*)str;
}



VkResult CreateDebugUtilsMessengerEXT(VkGPU* vkGPU, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	//pointer to the function, as it is not part of the core. Function creates debugging messenger
	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkGPU->instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != NULL) {
		return func(vkGPU->instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}


void DestroyDebugUtilsMessengerEXT(VkGPU* vkGPU, const VkAllocationCallbacks* pAllocator) {
	//pointer to the function, as it is not part of the core. Function destroys debugging messenger
	PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkGPU->instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != NULL) {
		func(vkGPU->instance, vkGPU->debugMessenger, pAllocator);
	}
}


static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	printf("validation layer: %s\n", pCallbackData->pMessage);
	return VK_FALSE;
}


VkResult
setup_DebugUtilsMessenger(VkInstance instance,
                     VkDebugUtilsMessengerEXT *debugUtilsMessenger)
{
	//function that sets up the debugging messenger 
	if (enableValidationLayers == 0) return VK_SUCCESS;

        VkDebugUtilsMessengerCreateInfoEXT
        debugUtilsMessengerCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            (const void*) NULL,
            (VkDebugUtilsMessengerCreateFlagsEXT) 0,
            (VkDebugUtilsMessageSeverityFlagsEXT) VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            (VkDebugUtilsMessageTypeFlagsEXT) VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            (PFN_vkDebugUtilsMessengerCallbackEXT) debugCallback,
            (void*) NULL };
 
	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	if (func != NULL) {
	        if (func(instance, &debugUtilsMessengerCreateInfo, NULL, debugUtilsMessenger) != VK_SUCCESS) {
	        	return VK_ERROR_INITIALIZATION_FAILED;
	        }
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	return VK_SUCCESS;
}


VkResult
check_ValidationLayer(){
	//check if validation layers are supported when an instance is created
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, NULL);

	VkLayerProperties* availableLayers = (VkLayerProperties*)malloc(sizeof(VkLayerProperties) * layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

	for (uint32_t i = 0; i < layerCount; i++) {
		if (strcmp("VK_LAYER_KHRONOS_validation", availableLayers[i].layerName) == 0) {
			free(availableLayers);
			return VK_SUCCESS;
		}
	}
	free(availableLayers);
	return VK_ERROR_LAYER_NOT_PRESENT;
}


VkResult
create_Instance(VkInstance *instance) 
{
	//create instance - a connection between the application and the Vulkan library 
	VkResult res = VK_SUCCESS;
	//check if validation layers are supported
	if (enableValidationLayers == 1) {
	        uint32_t layerCount = 0;
	        vkEnumerateInstanceLayerProperties(&layerCount, NULL);
	        VkLayerProperties* availableLayers = (VkLayerProperties*)malloc(sizeof(VkLayerProperties) * layerCount);
	        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);
                res = VK_ERROR_LAYER_NOT_PRESENT;
	        for (uint32_t i = 0; i < layerCount; i++) {
	        	if (strcmp("VK_LAYER_KHRONOS_validation", availableLayers[i].layerName) == 0) {
	        		res = VK_SUCCESS;
                                printf("\nfind Validation Layer\n");
                                break;
	        	}
	        }
	        free(availableLayers);
		if (res != VK_SUCCESS) return res;
	}
	//sample app information
	VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO,
                              (const void*) NULL,
                              (const char*) "Vulkan Transposition Test",
                              (uint32_t) 1.0,
                              (const char*) "VulkanTransposition",
                              (uint32_t) 1.0,
                              (uint32_t) VK_API_VERSION_1_0 };

        VkDebugUtilsMessengerCreateInfoEXT
        debugUtilsMessengerCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            (const void*) NULL,
            (VkDebugUtilsMessengerCreateFlagsEXT) 0,
            (VkDebugUtilsMessageSeverityFlagsEXT) VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            (VkDebugUtilsMessageTypeFlagsEXT) VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            (PFN_vkDebugUtilsMessengerCallbackEXT) debugCallback,
            (void*) NULL };

	VkInstanceCreateInfo instanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                 (const void*) NULL,
                                 (VkInstanceCreateFlags) 0,
                                 (const VkApplicationInfo*) &applicationInfo,
                                 (uint32_t) 0,
                                 (const char* const*) NULL,
                                 (uint32_t) 0,
                                 (const char* const*) NULL };

	//specify, whether debugging utils are required
	const char* const extensions = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
	if (enableValidationLayers == 1) {
		instanceCreateInfo.enabledExtensionCount = 1;
		instanceCreateInfo.ppEnabledExtensionNames = &extensions;
	}
	
        const char* const validationLayers = { "VK_LAYER_KHRONOS_validation" };
	if( enableValidationLayers == 1 ) {
		//query for the validation layer support in the instance
		instanceCreateInfo.enabledLayerCount = 1;
		instanceCreateInfo.ppEnabledLayerNames = &validationLayers;
		instanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugUtilsMessengerCreateInfo;
	}

	//create instance
	return vkCreateInstance(&instanceCreateInfo, NULL, instance);
}


VkResult
find_PhysicalDevice(VkInstance instance,
                    uint32_t deviceId,
                    VkPhysicalDevice* physicalDevice)
{
	//check if there are GPUs that support Vulkan and select one
	VkResult res = VK_SUCCESS;
	uint32_t deviceCount=0xFFFFFFFF;
	res = vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
	if (res != VK_SUCCESS) return res;
	if (deviceCount == 0)  return VK_ERROR_DEVICE_LOST;

	VkPhysicalDevice* devices = (VkPhysicalDevice*) malloc(sizeof(VkPhysicalDevice) * deviceCount);
	res = vkEnumeratePhysicalDevices(instance, &deviceCount, devices);
	if (res != VK_SUCCESS) return res;
	*physicalDevice = devices[deviceId];
	free(devices);
	return VK_SUCCESS;
}

VkResult
get_Compute_QueueFamilyIndex(VkPhysicalDevice physicalDevice,
                             uint32_t *queueFamilyIndex) 
{
	//find a queue family for a selected GPU, select the first available for use
	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);

	VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);

	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);
	uint32_t i = 0;
	for (; i < queueFamilyCount; i++) {
		VkQueueFamilyProperties props = queueFamilies[i];

		if (props.queueCount > 0 && (props.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
			break;
		}
	}
	free(queueFamilies);
	if (i == queueFamilyCount) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	*queueFamilyIndex = i;
	return VK_SUCCESS;
}

VkResult 
create_logicalDevice(VkPhysicalDevice physicalDevice,
                     uint32_t *queueFamilyIndex, 
                     VkDevice *logicalDevice,
                     VkQueue  *queue)
{
	//create logical device representation
	VkResult res = VK_SUCCESS;
	res = get_Compute_QueueFamilyIndex(physicalDevice, queueFamilyIndex);
	if (res != VK_SUCCESS) return res;
	float queuePriorities = 1.0;
	VkDeviceQueueCreateInfo
            deviceQueueCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                (const void*) NULL,
                (VkDeviceQueueCreateFlags) 0,
                (uint32_t) *queueFamilyIndex,
                (uint32_t) 1,
                (const float*) &queuePriorities };

	VkPhysicalDeviceFeatures physicalDeviceFeatures = { 0 };
	//physicalDeviceFeatures.shaderFloat64 = VK_TRUE;//this enables double precision support in shaders 

	VkDeviceCreateInfo
            deviceCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                (const void*) NULL,
                (VkDeviceCreateFlags) 0,
                (uint32_t) 1,
                (const VkDeviceQueueCreateInfo*) &deviceQueueCreateInfo,
                (uint32_t) 0,
                (const char* const*) NULL,
                (uint32_t) 0,
                (const char* const*) NULL,
                (const VkPhysicalDeviceFeatures*) &physicalDeviceFeatures};

	res = vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, logicalDevice);
	if (res != VK_SUCCESS) return res;
	vkGetDeviceQueue(*logicalDevice, *queueFamilyIndex, 0, queue);
	return res;
}

VkResult
create_ShaderModule(VkDevice device,
                    VkShaderModule* shaderModule,
                    uint32_t shaderID)
{
	//create shader module, using the SPIR-V bytecode
	VkResult res = VK_SUCCESS;
	char shaderPath[256];
	//this sample uses two compute shaders, that can be selected by passing an appropriate id
	switch (shaderID) {
	case 0:
		sprintf(shaderPath, "%stransposition_no_bank_conflicts.spv", SHADER_DIR);
		break;
	case 1:
		sprintf(shaderPath, "%stransposition_bank_conflicts.spv", SHADER_DIR);
		break;
	case 2:
		sprintf(shaderPath, "%stransfer.spv", SHADER_DIR);
		break;
	default:
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	uint32_t shaderFilelength;
	//read bytecode
	uint32_t* code = VkFFTReadShader(&shaderFilelength, shaderPath);
	VkShaderModuleCreateInfo shaderModuleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	shaderModuleCreateInfo.pCode = code;
	shaderModuleCreateInfo.codeSize = shaderFilelength;
	res = vkCreateShaderModule(device, &shaderModuleCreateInfo, NULL, shaderModule);
	free(code);
	return res;
}


VkResult 
create_App(VkDevice device,
           void*    appSpecializationConstantsLayout,
           uint32_t coalescedMemory,
           VkBuffer**   buffer,
           VkDeviceSize *bufferSize,
           uint32_t*    size,
           VkDescriptorPool      *descriptorPool,
           VkDescriptorSetLayout *descriptorSetLayout,
           VkDescriptorSet       *descriptorSet,
           const char* shaderFilename, 
           VkPipelineLayout *pipelineLayout,
           VkPipeline       *pipeline)
{//create an application interface to Vulkan. This function binds the shader to the compute pipeline, so it can be used as a part of the command buffer later

        VkResult res = VK_SUCCESS;
        uint32_t descriptorPoolSize_descriptorCount = 2;
	//we have two storage buffer objects in one set in one pool
	VkDescriptorPoolSize descriptorPoolSize = {(VkDescriptorType) VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                   (uint32_t) descriptorPoolSize_descriptorCount };

	const VkDescriptorType descriptorTypes[2] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                       (const void*) NULL,
                                       (VkDescriptorPoolCreateFlags) 0,
                                       (uint32_t) 1,
                                       (uint32_t) 1,
                                       (const VkDescriptorPoolSize*) &descriptorPoolSize };

	res = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, descriptorPool);
	if (res != VK_SUCCESS) return res;

	//specify each object from the set as a storage buffer
	VkDescriptorSetLayoutBinding* descriptorSetLayoutBindings = 
                                      (VkDescriptorSetLayoutBinding*) malloc(descriptorPoolSize_descriptorCount * sizeof(VkDescriptorSetLayoutBinding));
	for (uint32_t ii = 0; ii < descriptorPoolSize_descriptorCount; ++ii) {
		descriptorSetLayoutBindings[ii].binding            = (uint32_t) ii;
		descriptorSetLayoutBindings[ii].descriptorType     = (VkDescriptorType) descriptorTypes[ii];
		descriptorSetLayoutBindings[ii].descriptorCount    = (uint32_t) 1;
		descriptorSetLayoutBindings[ii].stageFlags         = (VkShaderStageFlags) VK_SHADER_STAGE_COMPUTE_BIT;
                descriptorSetLayoutBindings[ii].pImmutableSamplers = (const VkSampler*) NULL; 
	}

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                            (const void*) NULL,
                                            (VkDescriptorSetLayoutCreateFlags) 0,
                                            (uint32_t) descriptorPoolSize_descriptorCount,
                                            (const VkDescriptorSetLayoutBinding*) descriptorSetLayoutBindings};
        //create layout
	res = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, descriptorSetLayout);
	if (res != VK_SUCCESS) return res;
	free(descriptorSetLayoutBindings);

	//provide the layout with actual buffers and their sizes
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                        (const void*) NULL,
                                        (VkDescriptorPool) *descriptorPool,
                                        (uint32_t) 1,
                                        (const VkDescriptorSetLayout*) descriptorSetLayout};
	res = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, descriptorSet);
	if (res != VK_SUCCESS) return res;

	for (uint32_t jj = 0; jj < descriptorPoolSize_descriptorCount; ++jj) {

		VkDescriptorBufferInfo descriptorBufferInfo = { 0 };
		if (jj == 0) {
			descriptorBufferInfo.buffer = buffer[0][0];
			descriptorBufferInfo.range  = bufferSize[0];
			descriptorBufferInfo.offset = 0;
		}
		if (jj == 1) {
			descriptorBufferInfo.buffer = buffer[1][0];
			descriptorBufferInfo.range  = bufferSize[1];
			descriptorBufferInfo.offset = 0;
		}

		VkWriteDescriptorSet writeDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                         (const void*) NULL,
                                         (VkDescriptorSet) *descriptorSet,
                                         (uint32_t) jj,
                                         (uint32_t) 0,
                                         (uint32_t) 1,
                                         (VkDescriptorType) descriptorTypes[jj],
                                         (const VkDescriptorImageInfo*) NULL,
                                         (const VkDescriptorBufferInfo*) &descriptorBufferInfo,
                                         (const VkBufferView*) NULL };
		vkUpdateDescriptorSets((VkDevice) device,
                                       (uint32_t) 1,
                                       (const VkWriteDescriptorSet*) &writeDescriptorSet,
                                       (uint32_t) 0,
                                       (const VkCopyDescriptorSet*) NULL);
	}

        //specify how many push constants can be specified when the pipeline is bound to the command buffer
	VkPushConstantRange pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT,
                                (uint32_t) 0,
                                (uint32_t) sizeof(VkAppPushConstantsLayout) };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                       (const void*) NULL,             
                                       (VkPipelineLayoutCreateFlags) 0,
                                       (uint32_t) 1,
                                       (const VkDescriptorSetLayout*) descriptorSetLayout,
                                       (uint32_t) 1,
                                       (const VkPushConstantRange*) &pushConstantRange };
	//create pipeline layout
	res = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, pipelineLayout);
	if (res != VK_SUCCESS) return res;

        {
	        //specify specialization constants
                //- structure that sets constants in the shader after first compilation (done by glslangvalidator, for example)
                //  but before final shader module creation
	        //  first three values - workgroup dimensions 
	        ((VkAppSpecializationConstantsLayout*) appSpecializationConstantsLayout)->localSize[0] = coalescedMemory / sizeof(float);
	        ((VkAppSpecializationConstantsLayout*) appSpecializationConstantsLayout)->localSize[1] = coalescedMemory / sizeof(float);
	        ((VkAppSpecializationConstantsLayout*) appSpecializationConstantsLayout)->localSize[2] = 1;

	        //next three - buffer strides for multidimensional data
	        ((VkAppSpecializationConstantsLayout*) appSpecializationConstantsLayout)->inputStride[0] = 1;
	        ((VkAppSpecializationConstantsLayout*) appSpecializationConstantsLayout)->inputStride[1] = size[0];
	        ((VkAppSpecializationConstantsLayout*) appSpecializationConstantsLayout)->inputStride[2] = size[0] * size[1];
        }


	VkSpecializationMapEntry specializationMapEntries[6] = { 0 };
	for (uint32_t kk = 0; kk < 6; kk++) {
		specializationMapEntries[kk].constantID = kk + 1;
		specializationMapEntries[kk].size = sizeof(uint32_t);
		specializationMapEntries[kk].offset = kk * sizeof(uint32_t);
	}

	VkSpecializationInfo specializationInfo = { (uint32_t) 6,
                                                    (const VkSpecializationMapEntry*) specializationMapEntries,
                                                    (size_t) 6 * sizeof(uint32_t),
                                                    (const void*) appSpecializationConstantsLayout };

	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                            (const void*) NULL,
                                            (VkPipelineShaderStageCreateFlags) 0,
                                            (VkShaderStageFlagBits) VK_SHADER_STAGE_COMPUTE_BIT,
                                            (VkShaderModule) 0,
                                            (const char*) "main",
                                            (const VkSpecializationInfo*) &specializationInfo };
	{
            //create a shader module from the byte code
	    uint32_t shaderFilelength = 0;
	    //read bytecode
	    uint32_t* shaderModuleCode = VkFFTReadShader(&shaderFilelength, shaderFilename);
	    VkShaderModuleCreateInfo shaderModuleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                         (const void*) NULL,
                                         (VkShaderModuleCreateFlags) 0,
                                         (size_t) shaderFilelength,
                                         (const uint32_t*) shaderModuleCode };
	    res = vkCreateShaderModule(device, &shaderModuleCreateInfo, NULL, &pipelineShaderStageCreateInfo.module);
	    free( shaderModuleCode );
	    if (res != VK_SUCCESS) return res;
	}
	VkComputePipelineCreateInfo computePipelineCreateInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                        (const void*) NULL,
                                        (VkPipelineCreateFlags) 0,
                                        (VkPipelineShaderStageCreateInfo) pipelineShaderStageCreateInfo,
                                        (VkPipelineLayout) *pipelineLayout,
                                        (VkPipeline) NULL,
                                        (int32_t)    0 };
        //create pipeline
	res = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, NULL, pipeline);
	if (res != VK_SUCCESS) return res;

	vkDestroyShaderModule(device, pipelineShaderStageCreateInfo.module, NULL);
	return res;
}


void 
appendApp(VkGPU* vkGPU,
          VkApplication* app,
          VkCommandBuffer* commandBuffer)
{
	//this function appends to the command buffer: push constants, binds pipeline, descriptors, the shader's program dispatch call and the barrier between two compute stages to avoid race conditions 
	VkMemoryBarrier memory_barrier = {
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				0,
				VK_ACCESS_SHADER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT,
	};
	app->pushConstants.pushID = 0;
	//specify push constants - small amount of constant data in the shader
	vkCmdPushConstants(commandBuffer[0], app->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VkAppPushConstantsLayout), &app->pushConstants);
	//bind compute pipeline to the command buffer
	vkCmdBindPipeline(commandBuffer[0], VK_PIPELINE_BIND_POINT_COMPUTE, app->pipeline);
	//bind descriptors to the command buffer
	vkCmdBindDescriptorSets(commandBuffer[0], VK_PIPELINE_BIND_POINT_COMPUTE, app->pipelineLayout, 0, 1, &app->descriptorSet, 0, NULL);
	//record dispatch call to the command buffer - specifies the total amount of workgroups
	vkCmdDispatch(commandBuffer[0], app->size[0] / app->specializationConstants.localSize[0], app->size[1] / app->specializationConstants.localSize[1], app->size[2] / app->specializationConstants.localSize[2]);
	//memory synchronization between two compute dispatches
	vkCmdPipelineBarrier(commandBuffer[0], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memory_barrier, 0, NULL, 0, NULL);

}



VkResult
run_App(VkGPU* vkGPU,
        VkDevice device,
        VkCommandPool commandPool,
        VkCommandBuffer* commandBuffer,
        VkPipelineLayout pipelineLayout,
        VkQueue queue,
        VkFence *fence,
        VkApplication* app,
        uint32_t batch,
        double* time)
{
	VkResult res = VK_SUCCESS;
	//create command buffer to be executed on the GPU
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                        (const void*) NULL,
                                        (VkCommandPool) commandPool,
                                        (VkCommandBufferLevel) VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                        (uint32_t) 1 };
	VkCommandBuffer commandBuffer = {0};
	res = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);

	VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                     (const void*) NULL,
                                     (VkCommandBufferUsageFlags) VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                     (const VkCommandBufferInheritanceInfo*) NULL };
	//begin command buffer recording
	res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

	if (res != VK_SUCCESS) return res;
	//Record commands batch times. Allows to perform multiple operations in one submit to mitigate dispatch overhead
	for (uint32_t i = 0; i < batch; i++) {
		appendApp(vkGPU, app, &commandBuffer);

	        //this function appends to the command buffer: push constants, binds pipeline, descriptors,
                //the shader's program dispatch call and the barrier between two compute stages to avoid race conditions 
	        VkMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER,
	                            (const void*) NULL,
	                            (VkAccessFlags) VK_ACCESS_SHADER_WRITE_BIT,
	       	                    (VkAccessFlags) VK_ACCESS_SHADER_READ_BIT };
	        app->pushConstants.pushID = 0;
	        //specify push constants - small amount of constant data in the shader
	        vkCmdPushConstants(commandBuffer[0], pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VkAppPushConstantsLayout), &app->pushConstants);
	        //bind compute pipeline to the command buffer
	        vkCmdBindPipeline(commandBuffer[0], VK_PIPELINE_BIND_POINT_COMPUTE, app->pipeline);
	        //bind descriptors to the command buffer
	        vkCmdBindDescriptorSets(commandBuffer[0], VK_PIPELINE_BIND_POINT_COMPUTE, app->pipelineLayout, 0, 1, &app->descriptorSet, 0, NULL);
	        //record dispatch call to the command buffer - specifies the total amount of workgroups
	        vkCmdDispatch(commandBuffer[0], app->size[0] / app->specializationConstants.localSize[0], app->size[1] / app->specializationConstants.localSize[1], app->size[2] / app->specializationConstants.localSize[2]);
	        //memory synchronization between two compute dispatches
	        vkCmdPipelineBarrier(commandBuffer[0], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, NULL, 0, NULL);

	}
	//end command buffer recording
	res = vkEndCommandBuffer(commandBuffer);
	if (res != VK_SUCCESS) return res;


	//submit the command buffer for execution and place the fence after, measure time required for execution
	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	clock_t t;
	t = clock();
	res = vkQueueSubmit(vkGPU->queue, 1, &submitInfo, vkGPU->fence);
	if (res != VK_SUCCESS) return res;
	res = vkWaitForFences(vkGPU->device, 1, &vkGPU->fence, VK_TRUE, 100000000000);
	if (res != VK_SUCCESS) return res;
	t = clock() - t;
	time[0] = ((double)t) / CLOCKS_PER_SEC * 1000/batch; //in ms
	res = vkResetFences(vkGPU->device, 1, &vkGPU->fence);
	if (res != VK_SUCCESS) return res;
	//free the command buffer
	vkFreeCommandBuffers(vkGPU->device, vkGPU->commandPool, 1, &commandBuffer);
	return res;
}


void deleteApp(VkGPU* vkGPU, VkApplication* app) {
	//destroy previously allocated resources of the application
	vkDestroyDescriptorPool(vkGPU->device, app->descriptorPool, NULL);
	vkDestroyDescriptorSetLayout(vkGPU->device, app->descriptorSetLayout, NULL);
	vkDestroyPipelineLayout(vkGPU->device, app->pipelineLayout, NULL);
	vkDestroyPipeline(vkGPU->device, app->pipeline, NULL);
}


VkResult
find_MemoryType(VkPhysicalDevice physicalDevice,
                uint32_t memoryTypeBits,
                VkMemoryPropertyFlags memoryPropertyFlags,
                uint32_t* memoryTypeIndex)
{
	//find memory with specified properties
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties = { 0 };

	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

	for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; ++i) {
		if ((memoryTypeBits & (1 << i)) && ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags))
		{
			memoryTypeIndex[0] = i;
			return VK_SUCCESS;
		}
	}
	return VK_ERROR_INITIALIZATION_FAILED;
}


VkResult
allocate_Buffer_DeviceMemory(VkPhysicalDevice physicalDevice,
                             VkDevice logicalDevice, 
                             VkBufferUsageFlags bufferUsageFlags,
                             VkPhysicalDeviceMemoryProperties *physicalDeviceMemoryProperties,
                             VkMemoryPropertyFlags memoryPropertyFlags,
                             VkDeviceSize size,
                             VkBuffer *buffer,
                             VkDeviceMemory* deviceMemory )
{
	//allocate the buffer used by the GPU with specified properties
	VkResult res = VK_SUCCESS;
	uint32_t queueFamilyIndices;
	VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                               (const void*) NULL,
                               (VkBufferCreateFlags) 0,
                               (VkDeviceSize) size,
                               (VkBufferUsageFlags) bufferUsageFlags,
                               (VkSharingMode)  VK_SHARING_MODE_EXCLUSIVE,
                               (uint32_t) 1,
                               (const uint32_t*) &queueFamilyIndices };

	res = vkCreateBuffer(logicalDevice, &bufferCreateInfo, NULL, buffer);
	if (res != VK_SUCCESS) return res;

	VkMemoryRequirements memoryRequirements = { 0 };
	vkGetBufferMemoryRequirements(logicalDevice, buffer[0], &memoryRequirements);

	//find memory with specified properties
        uint32_t memoryTypeIndex = 0xFFFFFFFF;
        if (NULL == physicalDeviceMemoryProperties) vkGetPhysicalDeviceMemoryProperties(physicalDevice, physicalDeviceMemoryProperties);

	for (uint32_t i = 0; i < physicalDeviceMemoryProperties->memoryTypeCount; ++i) {
		if (( memoryRequirements.memoryTypeBits & (1 << i)) && ((physicalDeviceMemoryProperties->memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags))
		{
			memoryTypeIndex = i;
                        break;
		}
	}
	if(0xFFFFFFFF ==  memoryTypeIndex) return VK_ERROR_INITIALIZATION_FAILED;

	VkMemoryAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                 (const void*) NULL,
                                 (VkDeviceSize) memoryRequirements.size,
                                 (uint32_t) memoryTypeIndex };

	res = vkAllocateMemory(logicalDevice, &memoryAllocateInfo, NULL, deviceMemory);
	if (res != VK_SUCCESS) return res;

	res = vkBindBufferMemory(logicalDevice, buffer[0], deviceMemory[0], 0);
	return res;
}



VkResult
upload_Data(VkPhysicalDevice physicalDevice,
            VkDevice logicalDevice,
            void* data,
            VkPhysicalDeviceMemoryProperties *physicalDeviceMemoryProperties,
            VkCommandPool commandPool,
            VkQueue  queue,
            VkFence  *fence,
            VkBuffer *computeBuffer,
            VkDeviceSize bufferSize)
{
	VkResult res = VK_SUCCESS;

	//a function that transfers data from the CPU to the GPU using staging buffer,
        //because the GPU memory is not host-coherent
	VkDeviceSize stagingBufferSize = bufferSize;
	VkBuffer stagingBuffer = { 0 };
	VkDeviceMemory stagingBufferMemory = { 0 };
	res = allocate_Buffer_DeviceMemory(physicalDevice,
                                           logicalDevice,
                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                           physicalDeviceMemoryProperties,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                           stagingBufferSize,
                                           &stagingBuffer,
                                           &stagingBufferMemory );
	if (res != VK_SUCCESS) return res;

	void* stagingData;
	res = vkMapMemory(logicalDevice, stagingBufferMemory, 0, stagingBufferSize, 0, &stagingData);
	if (res != VK_SUCCESS) return res;
	memcpy(stagingData, data, stagingBufferSize);
	vkUnmapMemory(logicalDevice, stagingBufferMemory);


	        VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                (const void*) NULL,
                                                (VkCommandPool) commandPool,
                                                (VkCommandBufferLevel) VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                (uint32_t) 1 };
	        VkCommandBuffer commandBuffer = { 0 };
	        res = vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, &commandBuffer);
	        if (res != VK_SUCCESS) return res;



	        VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                             (const void*) NULL,
                                             (VkCommandBufferUsageFlags) VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                             (const VkCommandBufferInheritanceInfo*) NULL };
	        res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
	        if (res != VK_SUCCESS) return res;
	        VkBufferCopy copyRegion = { 0, 0, stagingBufferSize };
	        vkCmdCopyBuffer(commandBuffer, stagingBuffer, computeBuffer[0], 1, &copyRegion);
	        res = vkEndCommandBuffer(commandBuffer);
	        if (res != VK_SUCCESS) return res;



	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO,
                         (const void*) NULL,
                         (uint32_t) 0,
                         (const VkSemaphore*) NULL,
                         (const VkPipelineStageFlags*) NULL,
                         (uint32_t) 1,
                         (const VkCommandBuffer*) &commandBuffer,
                         (uint32_t ) 0,
                         (const VkSemaphore*) NULL };

	res = vkQueueSubmit(queue, 1, &submitInfo, *fence);
	if (res != VK_SUCCESS) return res;


	res = vkWaitForFences(logicalDevice, 1, fence, VK_TRUE, 100000000000);
	if (res != VK_SUCCESS) return res;

	res = vkResetFences(logicalDevice, 1, fence);
	if (res != VK_SUCCESS) return res;

	vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
	vkDestroyBuffer(logicalDevice, stagingBuffer, NULL);
	vkFreeMemory(logicalDevice, stagingBufferMemory, NULL);
	return res;
}


VkResult
download_Data(VkGPU* vkGPU,
              void* data,
              VkBuffer* buffer,
              VkDeviceSize bufferSize) 
{
	VkResult res = VK_SUCCESS;

	//a function that transfers data from the GPU to the CPU using staging buffer, because the GPU memory is not host-coherent
	VkDeviceSize stagingBufferSize = bufferSize;
	VkBuffer stagingBuffer = { 0 };
	VkDeviceMemory stagingBufferMemory = { 0 };
	res = allocate_Buffer_DeviceMemory(vkGPU->physicalDevice,
                                           vkGPU->device,
                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           &vkGPU->physicalDeviceMemoryProperties,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                           stagingBufferSize,
                                           &stagingBuffer,
                                           &stagingBufferMemory );
	if (res != VK_SUCCESS) return res;


	VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                        (const void*) NULL, 


                                          };
	commandBufferAllocateInfo.commandPool = vkGPU->commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;
	VkCommandBuffer commandBuffer = { 0 };
	res = vkAllocateCommandBuffers(vkGPU->device, &commandBufferAllocateInfo, &commandBuffer);
	if (res != VK_SUCCESS) return res;
	VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
	if (res != VK_SUCCESS) return res;
	VkBufferCopy copyRegion = { 0 };
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = stagingBufferSize;
	vkCmdCopyBuffer(commandBuffer, buffer[0], stagingBuffer, 1, &copyRegion);
	vkEndCommandBuffer(commandBuffer);
	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	res = vkQueueSubmit(vkGPU->queue, 1, &submitInfo, vkGPU->fence);
	if (res != VK_SUCCESS) return res;
	res = vkWaitForFences(vkGPU->device, 1, &vkGPU->fence, VK_TRUE, 100000000000);
	if (res != VK_SUCCESS) return res;
	res = vkResetFences(vkGPU->device, 1, &vkGPU->fence);
	if (res != VK_SUCCESS) return res;
	vkFreeCommandBuffers(vkGPU->device, vkGPU->commandPool, 1, &commandBuffer);


	void* stagingData;
	res = vkMapMemory(vkGPU->device, stagingBufferMemory, 0, stagingBufferSize, 0, &stagingData);
	if (res != VK_SUCCESS) return res;
	memcpy(data, stagingData, stagingBufferSize);
	vkUnmapMemory(vkGPU->device, stagingBufferMemory);


	vkDestroyBuffer(vkGPU->device, stagingBuffer, NULL);
	vkFreeMemory(vkGPU->device, stagingBufferMemory, NULL);
	return res;
}

VkResult
list_PhysicalDevice() {
    //this function creates an instance and prints the list of available devices
    VkResult res = VK_SUCCESS;
    VkInstance local_instance = {0};

    VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    createInfo.flags = 0;
    createInfo.pApplicationInfo = NULL;
    createInfo.enabledLayerCount = 0;
    createInfo.enabledExtensionCount = 0;
    createInfo.pNext = NULL;
    res = vkCreateInstance(&createInfo, NULL, &local_instance);
    if (res != VK_SUCCESS) return res;

    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
   
   	uint32_t deviceCount;
   	res = vkEnumeratePhysicalDevices(local_instance, &deviceCount, NULL);
   	if (res != VK_SUCCESS) return res;
   
   	VkPhysicalDevice* devices=(VkPhysicalDevice *) malloc(sizeof(VkPhysicalDevice)*deviceCount);
   	res = vkEnumeratePhysicalDevices(local_instance, &deviceCount, devices);
   	if (res != VK_SUCCESS) return res;
   	for (uint32_t i = 0; i < deviceCount; i++) {
   		VkPhysicalDeviceProperties device_properties;
   		vkGetPhysicalDeviceProperties(devices[i], &device_properties);
   		printf("\nDevice id: %d name: %s API:%d.%d.%d\n\n", i, device_properties.deviceName, (device_properties.apiVersion >> 22), ((device_properties.apiVersion >> 12) & 0x3ff), (device_properties.apiVersion & 0xfff));
   	}
   	free(devices);
   	vkDestroyInstance(local_instance, NULL);
   	return res;
}




VkResult
Example_VulkanTransposition(uint32_t deviceID,
           uint32_t coalescedMemory,
           uint32_t size)
{
	VkGPU vkGPU = { 0 };
	vkGPU.device_id = deviceID;
	VkResult res = VK_SUCCESS;


	//create instance - a connection between the application and the Vulkan library 
	res = create_Instance( &vkGPU.instance );
	if (res != VK_SUCCESS) {
		printf("Instance creation failed, error code: %d\n", res);
		return res;
	}

	//set up the debugging messenger 
	res = setup_DebugUtilsMessenger(vkGPU.instance, &vkGPU.debugMessenger);
	if (res != VK_SUCCESS) {
		printf("Debug utils messenger creation failed, error code: %d\n", res);
		return res;
	}

	//check if there are GPUs that support Vulkan and select one
	res = find_PhysicalDevice(vkGPU.instance, deviceID, &vkGPU.physicalDevice);
	if (res != VK_SUCCESS) {
		printf("Physical device not found, error code: %d\n", res);
		return res;
	}
        printf("\nPhysical device is found, return code: %d\n", res);

	//create logical device representation
	res = create_logicalDevice(vkGPU.physicalDevice, &vkGPU.queueFamilyIndex, &vkGPU.device, &vkGPU.queue);
	if (res != VK_SUCCESS) {
		printf("logical Device creation failed, error code: %d\n", res);
		return res;
	}
        printf("\nlogical Device creation succeed, return code: %d\n", res);

	//create fence for synchronization 
	VkFenceCreateInfo fenceCreateInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                              (const void*) NULL,
                              (VkFenceCreateFlags) 0 };
	res = vkCreateFence(vkGPU.device, &fenceCreateInfo, NULL, &vkGPU.fence);
	if (res != VK_SUCCESS) {
		printf("Fence creation failed, error code: %d\n", res);
		return res;
	}
        printf("\nFence creation succeed, return code: %d\n", res);


	//create a place, command buffer memory is allocated from
	VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                    (const void*) NULL,
                                    (VkCommandPoolCreateFlags) VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                    (uint32_t) vkGPU.queueFamilyIndex };
	res = vkCreateCommandPool(vkGPU.device, &commandPoolCreateInfo, NULL, &vkGPU.commandPool);
	if (res != VK_SUCCESS) {
		printf("Command Pool Creation failed, error code: %d\n", res);
		return res;
	}
	printf("\nCommand Pool Creation succeed, return code: %d\n", res);


	//get device properties and memory properties, if needed
	vkGetPhysicalDeviceProperties(vkGPU.physicalDevice, &vkGPU.physicalDeviceProperties);
	vkGetPhysicalDeviceMemoryProperties(vkGPU.physicalDevice, &vkGPU.physicalDeviceMemoryProperties);



	//create app template and set the system size, the amount of memory to coalesce
	VkApplication app = { 0 };
	app.size[0] = size;
	app.size[1] = size;
	app.size[2] = 1;
	//use default values if coalescedMemory = 0
	if (coalescedMemory == 0) {
		switch (vkGPU.physicalDeviceProperties.vendorID) {
		case 0x10DE://NVIDIA - change to 128 before Pascal
			app.coalescedMemory = 32;
			break;
		case 0x8086://INTEL
			app.coalescedMemory = 64;
			break;
		case 0x13B5://AMD
			app.coalescedMemory = 64;
			break;
		default:
			app.coalescedMemory = 64;
			break;
		}
	}
	else{
		app.coalescedMemory = coalescedMemory;
        }




	//allocate input and output buffers
	VkDeviceSize inputBufferSize=sizeof(float)* app.size[0] * app.size[1] * app.size[2];
	VkBuffer inputBuffer = { 0 };
	VkDeviceMemory inputBufferDeviceMemory = { 0 };

	VkDeviceSize outputBufferSize=sizeof(float) * app.size[0] * app.size[1] * app.size[2];
	VkBuffer outputBuffer = { 0 };
	VkDeviceMemory outputBufferDeviceMemory = { 0 };


//
//The most optimal memory has the VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT flag and is usually not accessible by the CPU on dedicated graphics cards. 
//The memory type that allows us to access it from the CPU may not be the most optimal memory type for the graphics card itself to read from.
//
	res = allocate_Buffer_DeviceMemory(vkGPU.physicalDevice,
                                           vkGPU.device,
                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           &vkGPU.physicalDeviceMemoryProperties,
                                           VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, //device local memory
                                           inputBufferSize,
                                           &inputBuffer,
                                           &inputBufferDeviceMemory );
	if (res != VK_SUCCESS) {
		printf("Input buffer allocation failed, error code: %d\n", res);
		return res;
	}
        printf("\nInput buffer allocation succeeds, return code: %d\n", res);


	res = allocate_Buffer_DeviceMemory(vkGPU.physicalDevice,
                                           vkGPU.device,
                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           &vkGPU.physicalDeviceMemoryProperties,
                                           VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, //device local memory
                                           outputBufferSize,
                                           &outputBuffer,
                                           &outputBufferDeviceMemory );
	if (res != VK_SUCCESS) {
		printf("Output buffer allocation failed, error code: %d\n", res);
		return res;
	}
        printf("\nOutput buffer allocation succeeds, return code: %d\n", res);

	//allocate input data on the CPU
	float* buffer_input = (float*)malloc(inputBufferSize);
	for (uint32_t k = 0; k < app.size[2]; k++) {
		for (uint32_t j = 0; j < app.size[1]; j++) {
			for (uint32_t i = 0; i < app.size[0]; i++) {
				buffer_input[ (i + j * app.size[0] + k * (app.size[0]) * app.size[1])] = (i + j * app.size[0] + k * (app.size[0]) * app.size[1]);
			}
		}
	}

	//transfer data to GPU staging buffer and thereafter
        //sync the staging buffer with GPU local memory
	upload_Data(vkGPU.physicalDevice,
                    vkGPU.device,
                    buffer_input,
                    &vkGPU.physicalDeviceMemoryProperties,
                    vkGPU.commandPool,
                    vkGPU.queue,
                    &vkGPU.fence,
                    &inputBuffer,
                    inputBufferSize);
	free(buffer_input);
        printf("\nUpload Data succeeds, return code: %d\n", res);


	//specify pointers in the app with the previously allocated buffers data
	app.inputBufferSize         = inputBufferSize;
	app.inputBuffer             = &inputBuffer;
	app.inputBufferDeviceMemory = &inputBufferDeviceMemory;
	app.outputBufferSize        = outputBufferSize;
	app.outputBuffer            = &outputBuffer;
	app.outputBufferDeviceMemory= &outputBufferDeviceMemory;

	//copy app for bank conflicted shared memory sample and bandwidth sample
	VkApplication app_bank_conflicts = app;
	VkApplication app_bandwidth      = app;

        VkBuffer*    buffer[2]     = {app.inputBuffer, app.outputBuffer };
        VkDeviceSize bufferSize[2] = {app.inputBufferSize, app.outputBufferSize };
        char shaderPath[256];
	//create transposition app with no bank conflicts from transposition shader
        sprintf(shaderPath, "%stransposition_no_bank_conflicts.spv", SHADER_DIR);
        res = create_App(vkGPU.device,
                         &(app.specializationConstants),                 
                         app.coalescedMemory,
                         buffer,
                         bufferSize,
                         app.size,
                         &app.descriptorPool,
                         &app.descriptorSetLayout,
                         &app.descriptorSet,
                         (const char*) shaderPath,
                         &app.pipelineLayout,
                         &app.pipeline );
	if (res != VK_SUCCESS) {
		printf("Application creation failed, error code: %d\n", res);
		return res;
	}
	printf("\nApplication with no bank conflicts from transposition shader creation succeeds, return code: %d\n", res);


	//create transposition app with bank conflicts from transposition shader
        sprintf(shaderPath, "%stransposition_bank_conflicts.spv", SHADER_DIR);
        res = create_App(vkGPU.device,
                         &(app_bank_conflicts.specializationConstants),                 
                         app_bank_conflicts.coalescedMemory,
                         buffer,
                         bufferSize,
                         app_bank_conflicts.size,
                         &app_bank_conflicts.descriptorPool,
                         &app_bank_conflicts.descriptorSetLayout,
                         &app_bank_conflicts.descriptorSet,
                         (const char*) shaderPath,
                         &app_bank_conflicts.pipelineLayout,
                         &app_bank_conflicts.pipeline );
	if (res != VK_SUCCESS) {
		printf("Application creation failed, error code: %d\n", res);
		return res;
	}
	printf("\nApplication with bank conflicts from transposition shader creation succeeds, return code: %d\n", res);


	//create bandwidth app, from the shader with only data transfers and no transposition
        sprintf(shaderPath, "%stransfer.spv", SHADER_DIR);
        res = create_App(vkGPU.device,
                         &(app_bandwidth.specializationConstants),                 
                         app_bandwidth.coalescedMemory,
                         buffer,
                         bufferSize,
                         app_bandwidth.size,
                         &app_bandwidth.descriptorPool,
                         &app_bandwidth.descriptorSetLayout,
                         &app_bandwidth.descriptorSet,
                         (const char*) shaderPath,
                         &app_bandwidth.pipelineLayout,
                         &app_bandwidth.pipeline );
	if (res != VK_SUCCESS) {
		printf("Application creation failed, error code: %d\n", res);
		return res;
	}
	printf("\nBandwidth Application with no transposition succeeds, return code: %d\n", res);

	double time_no_bank_conflicts = 0;
	double time_bank_conflicts = 0;
	double time_bandwidth = 0;

	//perform transposition with no bank conflicts on the input buffer and store it in the output 1000 times
	res = runApp(&vkGPU, &app, 1000, &time_no_bank_conflicts);
	if (res != VK_SUCCESS) {
		printf("Application 0 run failed, error code: %d\n", res);
		return res;
	}
	float* buffer_output = (float*)malloc(outputBufferSize);

	//Transfer data from GPU using staging buffer, if needed
	download_Data(&vkGPU, buffer_output, &outputBuffer, outputBufferSize);
	//Print data, if needed.
	/*for (uint32_t k = 0; k < app.size[2]; k++) {
		for (uint32_t j = 0; j < app.size[1]; j++) {
			for (uint32_t i = 0; i < app.size[0]; i++) {
				printf("%.6f ", buffer_output[i + j * app.size[0] + k * (app.size[0] * app.size[1])]);
			}
			printf("\n");
		}
		printf("\n");
	}*/
	//perform transposition with bank conflicts on the input buffer and store it in the output 1000 times
	res = runApp(&vkGPU, &app_bank_conflicts, 1000, &time_bank_conflicts);
	if (res != VK_SUCCESS) {
		printf("Application 1 run failed, error code: %d\n", res);
		return res;
	}

	//transfer data from the input buffer to the output buffer 1000 times
	res = runApp(&vkGPU, &app_bandwidth, 1000, &time_bandwidth);
	if (res != VK_SUCCESS) {
		printf("Application 2 run failed, error code: %d\n", res);
		return res;
	}
	//print results
	printf("Transpose time with no bank conflicts: %.3f ms\nTranspose time with bank conflicts: %.3f ms\nTransfer time: %.3f ms\nCoalesced Memory: %d bytes\nSystem size: %dx%d\nBuffer size: %d KB\nBandwidth: %d GB/s\nTranfer time/total transpose time: %0.3f%%\n",
            time_no_bank_conflicts,
            time_bank_conflicts,
            time_bandwidth,
            app.coalescedMemory,
            app.size[0],
            app.size[1],
            (int) inputBufferSize / 1024,
            (int)(2*1000*inputBufferSize / 1024.0 / 1024.0 / 1024.0 /time_bandwidth),
            time_bandwidth/ time_no_bank_conflicts *100);



	
	//free resources
	free(buffer_output);
	vkDestroyBuffer(vkGPU.device, inputBuffer, NULL);
	vkFreeMemory(vkGPU.device, inputBufferDeviceMemory, NULL);
	vkDestroyBuffer(vkGPU.device, outputBuffer, NULL);
	vkFreeMemory(vkGPU.device, outputBufferDeviceMemory, NULL);
	deleteApp(&vkGPU, &app);
	deleteApp(&vkGPU, &app_bank_conflicts);
	deleteApp(&vkGPU, &app_bandwidth);
	vkDestroyFence(vkGPU.device, vkGPU.fence, NULL);
	vkDestroyCommandPool(vkGPU.device, vkGPU.commandPool, NULL);
	vkDestroyDevice(vkGPU.device, NULL);
	DestroyDebugUtilsMessengerEXT(&vkGPU, NULL);
	vkDestroyInstance(vkGPU.instance, NULL);
	return res;
}

int main(int argc, char* argv[])
{
	uint32_t device_id = 1;      //device id used in application
	uint32_t coalescedMemory = 0;//how much memory is coalesced
	uint32_t size = 2048;

        list_PhysicalDevice();

	VkResult res = Example_VulkanTransposition(device_id, coalescedMemory, size);
	return res;
}


#ifdef __cplusplus
}
#endif

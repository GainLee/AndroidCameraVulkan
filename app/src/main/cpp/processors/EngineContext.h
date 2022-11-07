//
// Created by 李根 on 2022/10/10.
//

#ifndef PHOTOALGORITHM_ENGINECONTEXT_H
#define PHOTOALGORITHM_ENGINECONTEXT_H

#include <VulkanBufferWrapper.h>
#include <VulkanContext.h>
#include <VulkanDeviceWrapper.hpp>
#include <VulkanSwapChain.h>
#include <android/native_window.h>
#include <camera.hpp>
#include <chrono>
#include <jni.h>
#include <vulkan/vulkan.hpp>

class EngineContext {
private:
  void createSynchronizationPrimitives();
  void initSwapchain();
  void setupSwapChain();
  void createCommandBuffers();

protected:
  void initSyncObjects();

  virtual void createPipelines();

  virtual void prepareUniformBuffers();

  virtual void createDescriptorSet();

  virtual void buildCommandBuffers();

  // Create a semaphore with the managed device.
  bool createSemaphore(vk::Semaphore *semaphore) const;

  void setupDepthStencil();

  void setupFrameBuffer();

  void setupRenderPass();

  void prepareVertices(bool useStagingBuffers, const void *data,
                       size_t bufSize);

  vk::PipelineShaderStageCreateInfo loadShader(const char *shaderFilePath,
                                               vk::ShaderStageFlagBits stage);

  /** Prepare the next frame for workload submission by acquiring the next swap
   * chain image */
  void prepareFrame();
  /** @brief Presents the current image to the swap chain */
  void submitFrame();

  std::shared_ptr<VulkanContext> mVulkanContext;

  const std::shared_ptr<VulkanContext> vulkanContext() const {
    return mVulkanContext;
  }

  struct {
    ANativeWindow *nativeWindow;
    int32_t windowWidth;
    int32_t windowHeight;
  } mWindow;

  VulkanSwapChain mSwapChain;

  struct {
    vk::Image image;
    vk::DeviceMemory mem;
    vk::ImageView view;
  } depthStencil;

  vk::Format depthFormat;

  const char *vertFilePath;
  const char *fragFilePath;

  std::vector<vk::Framebuffer> frameBuffers;
  vk::RenderPass mRenderPass;

  vk::DescriptorPool mDescriptorPool = nullptr;

  vk::DescriptorSetLayout mDescriptorSetLayout = nullptr;
  vk::DescriptorSet mDescriptorSet;
  vk::PipelineLayout mPipelineLayout = nullptr;
  vk::Pipeline mPipeline = nullptr;

  // Synchronization primitives
  // Synchronization is an important concept of Vulkan that OpenGL mostly hid
  // away. Getting this right is crucial to using Vulkan.

  // Semaphores
  // Used to coordinate operations within the graphics queue and ensure correct
  // command ordering
  vk::Semaphore presentCompleteSemaphore = nullptr;
  vk::Semaphore renderCompleteSemaphore = nullptr;

  // Fences
  // Used to check the completion of queue operations (e.g. command buffer
  // execution)
  std::vector<vk::Fence> waitFences;

  // Active frame buffer index
  uint32_t currentBuffer = 0;

  // Command buffers used for rendering
  std::vector<vk::CommandBuffer> drawCmdBuffers;

  /** @brief Last frame time measured using a high performance timer (if
   * available) */
  float frameTimer = 1.0f;
  // Defines a frame rate independent timer value clamped from -1.0...1.0
  // For use in animations, rotations, etc.
  float timer = 0.0f;
  // Multiplier for speeding up (or slowing down) the global timer
  float timerSpeed = 1.0f;

  // Frame counter to display fps
  uint32_t frameCounter = 0;
  uint32_t lastFPS = 0;
  std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp;

  // Vertex buffer and attributes
  std::unique_ptr<vks::Buffer> mVerticesBuffer;

  // Uniform buffer block object
  std::unique_ptr<vks::Buffer> mUniformBuffer;

  struct {
    glm::mat4 projectionMatrix;
    glm::mat4 modelMatrix;
    glm::mat4 viewMatrix;
  } uboVS;

  Camera mCamera;

  bool mPrepared = false;

public:
  EngineContext(std::shared_ptr<VulkanContext> vulkanContext,
                const char *vertPath, const char *fragPath)
      : mVulkanContext(vulkanContext), vertFilePath(vertPath),
        fragFilePath(fragPath) {}

  virtual ~EngineContext();

  struct Settings {
    /** @brief Enable UI overlay */
    bool overlay = true;
    bool uesDepth = true;
  } settings;

  void connectSwapChain();

  void setNativeWindow(ANativeWindow *window, uint32_t width, uint32_t height);

  void windowResize();

  virtual void prepare(JNIEnv *env);

  virtual void draw();
};

#endif // PHOTOALGORITHM_ENGINECONTEXT_H

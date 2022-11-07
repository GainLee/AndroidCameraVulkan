//
// Created by Gain on 2022/8/31.
//

#ifndef GAINVULKANSAMPLE_SAMPLE_13_CAMERAHWB_H
#define GAINVULKANSAMPLE_SAMPLE_13_CAMERAHWB_H

#include "EngineContext.h"
#include <VulkanImageWrapper.h>

using namespace gain;

class Engine_CameraHwb : public EngineContext {
private:
  std::unique_ptr<Image> mImage;

  AHardwareBuffer *mBuffer;

  int mOrientation;

  virtual void createPipelines() override;

  virtual void createDescriptorSet() override;

  virtual void buildCommandBuffers() override;

  virtual void prepareUniformBuffers();

  void prepareSynchronizationPrimitives();

  void updateUniformBuffers();

  void setupDescriptorSetLayout();

  void setupDescriptorPool();

  void updateDescriptorSets();

  void prepareHdwImage();

  void updateTexture();

  void buildCommandBuffers(int index);

public:
  Engine_CameraHwb(std::shared_ptr<VulkanContext> vulkanContext)
      : mBuffer(nullptr),
        EngineContext(vulkanContext, "shaders/shader_13_camerahwb.vert.spv",
                      "shaders/shader_13_camerahwb.frag.spv") {
    settings.overlay = false;
    settings.uesDepth = false;
  }

  virtual void prepare(JNIEnv *env) override;

  virtual void draw();

  void setHdwImage(AHardwareBuffer *buffer, int orientation);

  ~Engine_CameraHwb();
};

#endif // GAINVULKANSAMPLE_SAMPLE_13_CAMERAHWB_H

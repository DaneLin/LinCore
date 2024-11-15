# LinCore 简介
&emsp;&emsp;一个基于vkguide教程的Vulkan代码实验平台，基于Vulkan1.3的新特性，将会实现教程中的所有功能，并且将会基于《Mastering Graphics Programming
with Vulkan》进一步拓展该引擎，并实现更多图形学算法。

A Vulkan code experimentation platform based on the vkguide tutorial, leveraging new features of Vulkan 1.3. It will implement all functionalities from the tutorial and further expand the engine based on 'Mastering Graphics Programming with Vulkan', incorporating additional graphics algorithms.

# 已实现特性 (Implemented Features)
- [x] 自动管线布局创建 Automating Pipeline Layout Creation 
  - [x] 通过反射自动创建管线布局 Automatically create pipeline layout through reflection
  - [x] 通过反射自动创建着色器模块 Automatically create shader module through reflection

# 待实现特性 (To-Do Features)
- [ ] 管线缓存 Pipeline Cache
- [ ] 无绑定渲染 Bindless Rendering
	- [ ] 无绑定纹理 Bindless Texture
	- [ ] 无绑定缓冲 Bindless Buffer
- [ ] 多线程渲染 Multi-threaded Rendering
- [ ] 阴影映射 Shadow Mapping
- [ ] 全局光照 Global Illumination

# References
## Tutorials
https://vkguide.dev/  
https://vulkan-tutorial.com/

## CodeBases
https://github.com/vblanco20-1/vulkan-guide.git  
https://github.com/zoheth/Xihe  
https://github.com/SaschaWillems/Vulkan.git  
https://github.com/KhronosGroup/Vulkan-Samples  
https://github.com/PacktPublishing/Mastering-Graphics-Programming-with-Vulkan.git

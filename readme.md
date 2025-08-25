# WCR Renderer Readme
##### WCR is hobby renderer that I've primarily worked on as a way to streamline experimenting with different graphics techniques
##### as such many of the choices made are geared around being much more general or towards kickstarting development as much
##### as possible

## Architecture
### Core Structures and Classes
#### Device
    The device abstracts the initialization process for vulkan as well as a means to communicate with the GPU without
    directly handling all of it's state. Instead of managing numerous different variables spread across multiple files,
    the device can be used as an interface to various functions that allow access to those variables when needed.
    Vulkan Memory Allocator is used to abstract managing GPU device memory. It's allocator can be access with a call to
    get_allocator()
    
    The raw vk::Device cam be accessed through a call to "get_handle()", while each respective queue in the renderer can
    be accessed through a call to it's respective acessor method:
    get_graphics_queue()
    get_present_queue()
    get_transferQueue()

    the draw images and swapchain also have their own respective methods for this perpose;

    Synchronization objects for immediate mode command submission can also be accessed through a the immediateInfo structure
### FrameInFlight
    Houses the command pool and command buffers for each frame in flight. It also houses a the semaphore that is use for
    to wait on swapchain image acquisition and the fence that blocks wait, reset, submit commands for each frame in flight

### Swapchain Image Data
    Houses the swapchain image, image view, and render end semaphore for each swapchain image.

### ImmediateInfo
    Holds a command pool, command buffer, and a fence that is seperate from any frame in flight that can be used to
    submit commands with only a singular fence seperating submissions.
#### Context
    The context structure acts as a means to communicate with the GPU by submiting work and by being able to create GPU side
    resources.
    
    Three kinds of work submissions are currently supported
    1.
        submit_work(const CommandBuffer &cmd,
            const vk::PipelineStageFlagBits2 wait,
            const vk::PipelineStageFlagBits2 signal,
            const vk::Semaphore acquiredSemaphore,
            const vk::Semaphore renderEndSemaphore,
            const vk::Fence renderFence)
                This function takes a semaphore for when a swapchain image is acquired along with a stage flag to indicate
                when the stage to wait on to try to acquire the next swapchain image. It also takes a renderEndSemaphore
                and a stage flag to correlate with signaling the semaphore when rendering a frame is finished. Finally it takes
                a "renderFence" that blocks submit, wait, and reset commands.

    2. submit_upload_work()
            this command submission function is utilizes the functionality for submitting immediate commands to submit upload
            workloads

    3. submit_immediate_work()
            this command takes any function as a function pointer that takes a CommandBuffer structure that records commands
            to said command buffer. A new CommandBuffer structure doesn't need to be provided as it will make use of the
            immediate mode submission command buffer and pool/ The command buffer will be reset, begun and ended automatically
            so that also doesn't need to be included in the function. The body of the function will be included between
            the begin() and end() call of the commands buffer and then will be synchronized using the immediate mode
            syncrhonization structures.

    Frame synchronization is done through the submit_frame() structure that takes any function that may consume a
    commandBufferInfo structure and SwapChainData structure. This function handles the synchronization between multiple different
    frames of flight automatically and you only need to provide a function to fill in how command recording and work submission
    will be accomplished.

#### Resource Data
        This structure houses each of the resources used to construct a scene in the renderer as well as metadata stored
        as unsigned 16 bit integers that are used to form handles to use outside of the resource management classes/structures.
        These resource types will be elaborated upon further in the "scene structures" section of this readme.
#### Scene Builder
        This structure manages taking in a scene description (at this moment the only scene description format that is supported
        is gltf file) and converts it into a Resource Data structure that the renderer can understand. It then will upload
        any GPU side data (like images, vertex buffers, etc.) to the GPU in one large immediate mode submission.
#### Scene Manager
#####        This structure take a resource data structure and acts as an interface for operating on it and accessing variable from it.
        To access a resource from this structure you can use one of the various accessor_methods to gain a handle to said resource.
        A the raw resource can still be accessed from a handle but it must passed into a function that will translate the handle into
        the resource that it points to. This should be done with some degree of caution however as this handle out a reference to the
        original object that may become null if the reference outlives in the object it points to. Use caution as well when passing handles
        to functions other than the methods of the Scene Manager for similar reasons.
#### Descriptor Builder
        This structure managers writing descriptors and updating descriptor sets. Since most storage and uniform buffers in
        the renderer are accessed using the buffer device adress extension, we don't end up having too many descriptors. The
        main two situations where descriptors still end up being relevant in the renderer is for the per frame uniform buffer
        "Scene Data" buffers and for images which are also accessed using descriptor indexing.
#### Pipeline Builder
        This struct mainly acts as a way to abstract a way building different graphics pipelines for different kinds of shaders.
        This structure hasn't been expanded upon as much as WCR still only supports Opaque rendering.

## Context resources
### Buffers
        Buffer create_buffer(const u64 allocationSize, vk::BufferUsageFlags usage, const VmaMemoryUsage memoryUsage, const VmaAllocationCreateFlags flags)

        CPU or GPU side buffer using VMA. All buffers include the buffer device address flag and said flag can be accesed through
        the address member of the buffer.

        A dedicated function for making CPU side staging buffers also exists
        Buffer create_staging_buffer(const u64 size)

### Images
        Image create_image(vk::Extent3D size, const VkFormat format, const VkImageUsageFlags usage, const u32 mipLevels, const bool mipmapped)

        Simple images constructed using VMA,

### Samplers

        A vk::Sampler that also stores it's mag and min filter with it
        Sampler Context::create_sampler(const vk::Filter minFilter, const vk::Filter magFilter, const vk::SamplerMipmapMode mipmapMode)

### Shader
        Shader Context::create_shader(std::string_view filePath)
        Takes a path to a SPIR-V binary to convert into a vk::ShaderModule

## Scene Structures
#### Mesh and Surfaces
    A surface holds an initial index and count to define the range of indices used to represent a renderable surface.
    It also holds the index of the material used to shade said surface along with it's axis aligned bounding box that is used
    for view frustum culling. A GPU surface differs slightly from a CPU side surface as a GPU surface may also include
    it's own render matrix unlike a CPU side matrix. A mesh simply acts as a collection of surfaces.

#### Materials
    A material houses the paramaters used for rendering a surface. A base colour is store as a 4 dimensional vector, while
    metalnness roughness pipeline parameters and emmissive values are stored as simple 32 bit floating point scalars. A number
    of texture handles (or unsigned 32 bit integers in the case of a GPU side material) are store to act as indices into the
    array of textures used in the PBR fragement shaders to shade the material.

#### Lights
    Lights store 16 bit aligned vectors for their position and colour along with various 32 bit floating point scalars for
    their other parameters. At the moment all lights act as point lights and light shading is purly cumulative and vary inefficient.
    In the future clustured shading should be implemented to signifigantly reduce the negative impact a large number of lights
    may have on peformance and different kinds of light sources should be supported.

### Nodes
    Nodes act as the basic key structure for representing the scene hirearchy and propagating transformations from parent to
    child in a scene. Each Node hodes a handle to it's parent, and a list of handles to it's children and calling the refresh()
    function will recursively propagate any transformation you apply to the parent unto it's children until a root is reached.
    Each scene also contains a list of nodes categorized out of bound by their type. Instead of checking if a node has a mesh
    or a light, nodes are instead binned into these different lists upon scene creation. If you want nodes only of a particular
    type, you should instead get nodes from the appropriate bin of NodeHandles. So if you wanted only nodes that have renderable
    meshes, you should refer to the renderable nodes. Nodes are also binned by material type. Currently the main two material
    types are opaqueNodes and transparentNodes, though support for rendering transparentNodes is not yet implemented.

### Textures
    Textures from an implementation stanpoint are just the normal Image structure but they are specifically stored using 
    VK_FORMAT_BC7_SRGB_BLOCK and all textures are represented as a large array of combined image samplers. As such the device
    used should support descriptor indexing. Only ktx2 images are supported so any images in any scene description should be
    converted to ktx2 images ahead of time.
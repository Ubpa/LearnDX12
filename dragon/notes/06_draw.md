# 06. 利用 Direct3D 绘制集合体

## 6.1 顶点与输入布局

类似于 opengl 的 layout，但 DX 需要手动声明

输入布局是**输入元素布局**的数组

输入元素布局为

```c++
struct D3D12_INPUT_ELEMENT_DESC {
    LPCSTR SemanticName; // 语义名
    UINT SemanticIndex;  // 语义名索引,如 TEXCOORD0, TEXCOORD1
    DXGI_FORMAT Format;  // 格式，如 R32G32B32_FLOAT
    UINT InputSlot;      // 输入槽索引，0 - 15
    UINT AlignedByteOffset; // 偏移
    D3D12_INPUT_CLASSIFICATION InputSlotClass; // 输入槽类型，分 VERTEX 和 INSTANCE
    UINT InstanceDataStepRate; // VERTEX 时为 0，INSTANCE 时一般为 1
}
```

> **资源设置的逻辑** 
>
> PSO 设置了管线的大部分状态
>
> buffer 是具体的输入数据，不在 PSO 中
>
> buffer 分 VertexBuffer（多个）和 IndexBuffer（一个） 
>
> 用 stride 来确定 buffer 中的元素（顶点数据 / 图元索引）个数
>
> IndexBuffer 中的一个元素由 Format(u16 / u32) 和图元类型（点，线，三角形等）决定，不在 PSO 中
>
> VertexBuffer 中的一个元素由 InputLayout 决定，在 PSO 中

## 6.2 顶点缓冲区

缓冲就是简单的内存块（ID3D12Resource），当给缓冲一个视图后，它就变成了顶点/索引缓冲。

顶点缓冲视图

```c++
struct D3D12_VERTEX_BUFFER_VIEW
{
    D3D12_GPU_VIRTUAL_ADDRESS BufferLocation; // 地址（指针），通过 GetGPUVirtualAddress 获得
    UINT SizeInBytes; // 总大小
    UINT StrideInBytes; // 顶点元素大小
}
```

一般不需要更新，此时 buffer 放在 default 堆，并用 upload 堆进行初始化，用到了 UpdaateSubresources

用 IASetVertexBuffer 绑定 buffer 到对应的槽

用 DrawInstanced 绘制，此时只需要提供一个实例有多少个顶点

> 图元类型用 IASetPrimitiveTopology 设置

## 6.3 索引和索引缓冲区

类似于顶点缓冲，只是我们用 IMDEX_BUFFER_VIEW 来描述缓冲

IASetIndexBuffer 绑定索引缓冲

DrawIndexedInstanced 绘制含索引实体

## 6.4 顶点着色器示例

类似于 glsl

顶点着色器的输入输出与像素着色器的输入都需要用语义来描述

SV_POSITION 的齐次坐标用于裁剪

顶点着色器的输入构成输入签名，与输入布局相关联（映射关系）

## 6.5 像素着色器示例

返回值语义为 SV_TARGET

## 6.6 常量缓冲区

`cbuffer` 

每帧 CPU 更新一次，放到上传堆，大小应为 256B 的整数倍

同于顶点/索引缓冲区，需要一个视图 CBV

新语法

```c++
struct ObjectConstants
{
  float4x4 gWorldViewProj;
  uint matIndex;
};
ConstantBuffer<ObjectConstants> gObjConstants : register(b0);

//...

gObjConstants.matIndex
```

通过内存映射实现更新数据

常量缓冲区需要一个描述符堆来存放 CBV

根签名描述了管线的资源绑定（常量缓冲区，着色器资源视图（纹理），采样器，无序访问视图），由一组根参数构成，根参数可为

- 根常量
- 根描述符
- 描述符表

根签名在 PSO 中

> 根签名确定了寄存器槽的绑定方式
>
> 把资源绑定看成是一个函数，那么根签名就是该函数的参数列表
>
> 根签名相当于根参数数组，根参数对应参数列表里的参数
>
> 根参数有三种：常量，描述符，描述符表
>
> 一个描述符表可以和一个和多个寄存器相关联

## 6.7 编译着色器

`D3DCompileFromFile` 

可离线编译得到 cso

fxc 命令行工具

## 6.8 光栅器状态

渲染管线中大多阶段可编程，部分只能配置

光栅器状态就是关于光栅化的配置

```c++
struct D3D12_RASTERIZER_DESC
{
    D3D12_FILL_MODE FillMode;
    D3D12_CULL_MODE CullMode;
    BOOL FrontCounterClockwise;
    INT DepthBias;
    FLOAT DepthBiasClamp;
    FLOAT SlopeScaledDepthBias;
    BOOL DepthClipEnable;
    BOOL MultisampleEnable;
    BOOL AntialiasedLineEnable;
    UINT ForcedSampleCount;
    D3D12_CONSERVATIVE_RASTERIZATION_MODE ConservativeRaster;
}
```

## 6.9 流水线状态对象

Pipeline State Object PSO

```c++
struct D3D12_GRAPHICS_PIPELINE_STATE_DESC
{
    ID3D12RootSignature *pRootSignature; // 根签名
    D3D12_SHADER_BYTECODE VS; // 顶点着色器
    D3D12_SHADER_BYTECODE PS; // 像素着色器
    D3D12_SHADER_BYTECODE DS; // 域着色器
    D3D12_SHADER_BYTECODE HS; // 外壳着色器
    D3D12_SHADER_BYTECODE GS; // 几何着色器
    D3D12_STREAM_OUTPUT_DESC StreamOutput; // 流输出
    D3D12_BLEND_DESC BlendState; // 混合
    UINT SampleMask; // 采样开关，默认 0xffffffff
    D3D12_RASTERIZER_DESC RasterizerState; // 光栅器状态
    D3D12_DEPTH_STENCIL_DESC DepthStencilState; // 深度模板状态
    D3D12_INPUT_LAYOUT_DESC InputLayout; // 输入布局
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue; // ?
    D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType; // 图元类型（点线三角形四边形）
    UINT NumRenderTargets; // 渲染目标个数
    DXGI_FORMAT RTVFormats[ 8 ]; // 渲染目标格式
    DXGI_FORMAT DSVFormat; // 深度模板格式
    DXGI_SAMPLE_DESC SampleDesc; // 采样描述（数量和质量）
    UINT NodeMask; // ?
    D3D12_CACHED_PIPELINE_STATE CachedPSO; // ?
    D3D12_PIPELINE_STATE_FLAGS Flags; // ?
}
```

不是所有状态都在 PSO 中

一般为多个物体共用一个 PSO


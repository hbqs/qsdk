# QSDK
qsdk
河北骑士智能科技有限公司基于RT-THREAD RTOS编写的NB-IOT模块驱动代码，目前支持的模块型号有M5310、M5310A、ME3616。
源码地址：[https://github.com/hbqs/qsdk]。作者:[longmain](https://github.com/hbqs)


## 前言
由于目前各方都在大力推动NB-IOT产业发展，我们骑士智能科技也代理了NB-IOT模组，为了让大家在使用我们代理的模组时更加方便，我们编写了QSDK（NB-IOT驱动）代码，目前该代码在使用过程中简单方便，目前适配了RTTHREAD 组件，可以图像化配置，减轻了大量的工作，可以促进产品快速开发。
在这里感谢RT-THREAD 这款好用的的物联网系统，同时更感谢广大开源的支持者。让我从中学到了很多，网络也是一个好平台，希望所有的开发者能形成良性循环，从网络中学知识，回馈到网络中去。
## qsdk简介
qsdk是一个灵活的NB-IOT驱动，目前支持的模块型号有M5310、M5310A、ME3616，后期将会增加更多模组支持。该代码根据不同的模块启用不同的功能，目前代码支持电信IOT平台、中移ONENET平台（LWM2M协议和MQTT协议）和UDP\TCP协议。
在qsdk_io.c和qsdk_io.h代码中有控制NB模组引脚的函数需要移植，在qsdk_callback.c代码中是关于IOT、onenet、NET等下发处理回调函数，用户可以在相应函数中处理数据。
##  特点
qsdk开放源码，nb控制块、协议处理块均采用数据结构方式，网络数据下发采用回调函数机制，单独的qsdk_callback.h和qsdk_callback.c可以完整的提示网络下发数据，用户可以在函数里面编写对网络下发数据的处理逻辑代码。
同时代码支持快速初始化NB模块联网，快速连接onenet平台等功能，大大简化了客户的使用方式。

## qsdk代码在env使用
目前我们将NB-IOT模块驱动代码做成软件包（packages），如果使用RT-Thread操作系统的话，可以在env中直接配置使用，使用前请开启AT组建支持!

步骤如下：

1. **选择在线软件包**

![](https://github.com/hbqs/qsdk/help/png/1.png?raw=true)

2. **选择软件包属性为物联网相关**

![](https://github.com/hbqs/qsdk/help/png/2.png?raw=true)

3. **选择qsdk组件**

![](https://github.com/hbqs/qsdk/help/png/3.png?raw=true)

4. **进入qsdk的选项配置（自带默认属性）**

![](https://github.com/hbqs/qsdk/help/png/4.png?raw=true)

5. **为QSDK选择一款模组**

![](https://github.com/hbqs/qsdk/help/png/5.png?raw=true)

6. **更新软件包**

![](https://github.com/hbqs/qsdk/help/png/6.png?raw=true)

7. **编译生成mdk/iar工程**

![](https://github.com/hbqs/qsdk/help/png/7.png?raw=true)



## 联系人

* 邮箱：[longmain@longmain.cn](mailto:longmain@longmain.cn)
* 主页：[qsdk](https://github.com/hbqs)
* 仓库：[Github](https://github.com/hbqs)

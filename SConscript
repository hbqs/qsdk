from building import *

cwd = GetCurrentDir()
path = [cwd + '/inc']

src = Split('''
    src/qsdk_nb.c
	src/qsdk_callback.c
    ''')

if GetDepend(['QSDK_USING_NET']):
    src += Glob('src/qsdk_net.c')

if GetDepend(['QSDK_USING_ONENET']):
    src += Glob('src/qsdk_onenet.c')
    
if GetDepend(['QSDK_USING_IOT']):
    src += Glob('src/qsdk_iot.c')
    
if GetDepend(['QSDK_USING_MQTT']):
    src += Glob('src/qsdk_mqtt.c')

if GetDepend(['QSDK_USING_GPS']):
    src += Glob('src/qsdk_gps.c')
    
group = DefineGroup('qsdk', src, depend = ['PKG_USING_QSDK'], CPPPATH = path)

Return('group')

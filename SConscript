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

group = DefineGroup('QSDK', src, depend = ['PKG_USING_QSDK'], CPPPATH = path)

Return('group')

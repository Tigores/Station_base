#include <unistd.h>
#include <stdio.h>
#include <list>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "./Include/Common/dhnetsdk.h"
#include <cstdio>
#include <cstring>
#include <string.h>
#include <ctime>
#include <json/json.h>
#include <httplib.h>


#pragma comment(lib , "dhnetsdk.lib")

static BOOL g_bNetSDKInitFlag = FALSE;
static LLONG g_lLoginHandle = 0L;
static LLONG g_lRealLoadHandle = 0L;
static char g_szDevIp[32] = "192.168.137.145";
static int g_nPort = 37777; // TCP 连接端口，需与期望登录设备页面 TCP 端口配置一致
static char g_szUserName[64] = "admin";
static char g_szPasswd[64] = "lh1014qq";

//*********************************************************************************
// 常用回调集合声明
// 设备断线回调函数
typedef void (CALLBACK *fDisConnect)(long lLoginID, char *pchDVRIP, int nDVRPort, long dwUser);
void CALLBACK DisConnectFunc(long lLoginID, char *pchDVRIP, int nDVRPort, long dwUser);

// 断线重连成功回调函数
void CALLBACK HaveReConnect(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser);

// 智能分析数据回调
int CALLBACK AnalyzerDataCallBack(LLONG lAnalyzerHandle, DWORD dwAlarmType, void *pAlarmInfo, BYTE *pBuffer, DWORD dwBufSize, LDWORD dwUser, int nSequence, void *reserved);

//*********************************************************************************
void InitTest()
{
    // 初始化 SDK
    g_bNetSDKInitFlag = CLIENT_Init((fDisConnect)DisConnectFunc, 0);
    if (FALSE == g_bNetSDKInitFlag)
    {
        printf("Initialize client SDK fail; \n");
        return;
    }
    else
    {
        printf("Initialize client SDK done; \n");
    }
 
    // 获取 SDK 版本信息
    DWORD dwNetSdkVersion = CLIENT_GetSDKVersion();
    printf("NetSDK version is [%d]\n", dwNetSdkVersion);

    // 设置断线重连回调接口
    CLIENT_SetAutoReconnect(&HaveReConnect, 0);

    // 设置登录超时时间和尝试次数
    int nWaitTime = 5000; // 登录请求响应超时时间设置为 5s
    int nTryTimes = 3; // 登录时尝试建立链接 3 次
    CLIENT_SetConnectTime(nWaitTime, nTryTimes);
 
    // 设置更多网络参数
    NET_PARAM stuNetParm = {0};
    stuNetParm.nConnectTime = 3000; // 登录时尝试建立链接的超时时间
    CLIENT_SetNetworkParam(&stuNetParm);

    NET_IN_LOGIN_WITH_HIGHLEVEL_SECURITY stInparam;
    memset(&stInparam, 0, sizeof(stInparam));
    stInparam.dwSize = sizeof(stInparam);
    strncpy(stInparam.szIP, g_szDevIp, sizeof(stInparam.szIP) - 1);
    strncpy(stInparam.szPassword, g_szPasswd, sizeof(stInparam.szPassword) - 1);
    strncpy(stInparam.szUserName, g_szUserName, sizeof(stInparam.szUserName) - 1);
    stInparam.nPort = g_nPort;
    stInparam.emSpecCap = EM_LOGIN_SPEC_CAP_TCP;

    NET_OUT_LOGIN_WITH_HIGHLEVEL_SECURITY stOutparam;
    memset(&stOutparam, 0, sizeof(stOutparam));
    stOutparam.dwSize = sizeof(stOutparam);

    while(0 == g_lLoginHandle)
    {
        // 登录设备
        g_lLoginHandle = CLIENT_LoginWithHighLevelSecurity(&stInparam, &stOutparam);
        if (0 == g_lLoginHandle)
        {
            // 根据错误码，可以在 dhnetsdk.h 中找到相应的解释，此处打印的是 16 进制，头文件中是十进制，其中的转换需注意
            // 例如： #define NET_NOT_SUPPORTED_EC(23) // 当前 SDK 未支持该功能，对应的错误码为 0x80000017, 23 对应的 16 进制为 0x17
            printf("CLIENT_LoginWithHighLevelSecurity %s[%d]Failed!Last Error[%x]\n", g_szDevIp, g_nPort, CLIENT_GetLastError());
        }
        else
        {
            printf("CLIENT_LoginWithHighLevelSecurity %s[%d] Success\n", g_szDevIp, g_nPort);
        }
        // 用户初次登录设备，需要初始化一些数据才能正常实现业务功能，建议登录后等待一小段时间，具体等待时间因设备而异
        sleep(1);
        printf("\n");
    }
}

void RunTest()
{
    if (FALSE == g_bNetSDKInitFlag)
    {
        return;
    }
    
    if (0 == g_lLoginHandle)
    {
        return;
    }

    // 订阅智能图片报警
    LDWORD dwUser = 0;
    int nChannel = 0;
    // 每次设置对应一个通道，并且对应一种类型的事件
    // 如果要设置该通道上传所有类型的事件，可以将参数 dwAlarmType 设置为 EVENT_IVS_ALL
    // 如果需要设置一个通道上传两种事件，那么请调用两次 CLIENT_RealLoadPictureEx，并且传入不同的事件类型
    g_lRealLoadHandle = CLIENT_RealLoadPictureEx(g_lLoginHandle, nChannel, EVENT_IVS_ACCESS_CTL, TRUE, AnalyzerDataCallBack, dwUser, NULL);
    if (0 == g_lRealLoadHandle)
    {
        printf("CLIENT_RealLoadPictureEx Failed!Last Error[%x]\n", CLIENT_GetLastError());
        return;
    }
}

void EndTest()
{
    printf("input any key to quit!\n");
    getchar();

    // 停止订阅图片报警
    if (0 != g_lRealLoadHandle)
    {
        if (FALSE == CLIENT_StopLoadPic(g_lRealLoadHandle))
        {
            printf("CLIENT_StopLoadPic Failed!Last Error[%x]\n", CLIENT_GetLastError());
        }
        else
        {
            g_lRealLoadHandle = 0;
        }
    }

    // 退出设备
    if (0 != g_lLoginHandle)
    {
        if (FALSE == CLIENT_Logout(g_lLoginHandle))
        {
            printf("CLIENT_Logout Failed!Last Error[%x]\n", CLIENT_GetLastError());
        }
        else
        {
            g_lLoginHandle = 0;
        }
    }

    // 清理初始化资源
    if (TRUE == g_bNetSDKInitFlag)
    {
        CLIENT_Cleanup();
        g_bNetSDKInitFlag = FALSE;
    }
    return;
}

int main()
{
    InitTest();
    RunTest();
    EndTest();
    return 0;
}

//*********************************************************************************
// 常用回调集合定义
void CALLBACK DisConnectFunc(long lLoginID, char *pchDVRIP, int nDVRPort, long dwUser)
{
    printf("Call DisConnectFunc\n");
    printf("lLoginID[0x%lx]", lLoginID);
    if (NULL != pchDVRIP)
    {
        printf("pchDVRIP[%s]\n", pchDVRIP);
    }
    printf("nDVRPort[%d]\n", nDVRPort);
    printf("dwUser[%p]\n", (void*)dwUser); 
    printf("\n");
}

void CALLBACK HaveReConnect(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser)
{
    printf("Call HaveReConnect\n");
    printf("lLoginID[0x%lx]", lLoginID);
    if (NULL != pchDVRIP)
    {
        printf("pchDVRIP[%s]\n", pchDVRIP);
    }
    printf("nDVRPort[%d]\n", nDVRPort);
    printf("dwUser[%p]\n", (void*)dwUser); 
    printf("\n");
}

int CALLBACK AnalyzerDataCallBack(LLONG lAnalyzerHandle, DWORD dwAlarmType, void* pAlarmInfo, BYTE *pBuffer, DWORD dwBufSize, LDWORD dwUser, int nSequence, void *reserved)
{
    if (lAnalyzerHandle != g_lRealLoadHandle)
    {
        return 0;
    }

    int nAlarmChn = 0;
    switch(dwAlarmType)
    {
        case EVENT_IVS_ACCESS_CTL:
            {
                printf("EVENT_IVS_ACCESS_CTL event\n");
                DEV_EVENT_ACCESS_CTL_INFO* pStuInfo = (DEV_EVENT_ACCESS_CTL_INFO*)pAlarmInfo;
                nAlarmChn = pStuInfo->nChannelID;
                printf("nChannelID[%d]\n", pStuInfo->nChannelID);

                // 打印事件ID
                printf("Event ID: %d\n", pStuInfo->nEventID);

                // 打印门禁事件类型
                printf("Access Control Event Type: %d\n", pStuInfo->emEventType);

                // 打印卡号
                printf("Card Number: %s\n", pStuInfo->szCardNo);

                // 打印用户ID
                printf("User ID: %s\n", pStuInfo->szUserID);

                // 打印卡名
                printf("Card Name: %s\n", pStuInfo->szCardName);

                // 获取并打印当前时间
                time_t now;
                time(&now);
                char szTime[128] = "";
                strftime(szTime, sizeof(szTime) - 1, "%Y-%m-%d %H:%M:%S", localtime(&now));
                printf("Current Time: %s\n", szTime);

                // 创建以用户 ID 命名的文件夹
                char szFolderPath[256] = "";
                snprintf(szFolderPath, sizeof(szFolderPath), "/home/jetson/dxh/C/query_test/%s", pStuInfo->szUserID);
                mkdir(szFolderPath, 0755);

                // 处理第一张和第三张抓拍图像
                for (int i = 0; i < pStuInfo->nImageInfoCount; i++)
                {
                    if (i != 0 && i != 2)
                    {
                        continue; // 仅处理第一张（索引0）和第三张（索引2）
                    }

                    DEV_ACCESS_CTL_IMAGE_INFO* pImageInfo = &pStuInfo->stuImageInfo[i];
                    if (pImageInfo->nLength > 0)
                    {
                        char szImagePath[256] = "";
                        // time_t stuTime;
                        // time(&stuTime);
                        // char szTmpTime[128] = "";
                        // strftime(szTmpTime, sizeof(szTmpTime) - 1, "%y%m%d_%H%M%S", gmtime(&stuTime));
                        // snprintf(szImagePath, sizeof(szImagePath)-1, "%s/face_%d_%s_%d.jpg", szFolderPath, nAlarmChn, szTmpTime, i);
                        snprintf(szImagePath, sizeof(szImagePath)-1, "%s/face_%s_%d.jpg", szFolderPath, szTime, i);

                        // 使用偏移量和长度来读取图像数据
                        BYTE* pImageData = pBuffer + pImageInfo->nOffSet;
                        // if (pImageData == NULL) {
                        //     printf("Image data is NULL\n");
                        //     continue;
                        // }
                        // printf("Saving image to: %s\n", szImagePath); // 调试信息

                        FILE* pFile = fopen(szImagePath, "wb");
                        if (pFile != NULL)
                        {
                            fwrite(pImageData, 1, pImageInfo->nLength, pFile);
                            fclose(pFile);
                            printf("Saved face image to %s\n", szImagePath);
                        }
                        else
                        {
                            perror("Failed to save face image\n");
                        }
                    }
                    else
                    {
                        printf("No face image available\n");
                    }
                }

                // 打印抓拍照片存储地址
                printf("Snap URL: %s\n", pStuInfo->szSnapURL);
                // 构建 JSON 数据
                Json::Value root;
                root["UserID"] = pStuInfo->szUserID;
                root["CardName"] = pStuInfo->szCardName;
                root["CurrentTime"] = szTime;

                Json::StreamWriterBuilder writer;
                std::string json_data = Json::writeString(writer, root);

                // 发送 HTTP 请求
                httplib::Client cli("http://10.3.1.180:8080");
                auto res = cli.Post("/api/openApi/sendAc", json_data, "application/json");

                if (res && res->status == 200)
                {
                    std::cout << "Data sent successfully." << std::endl;
                }
                else
                {
                    std::cout << "Failed to send data." << std::endl;
                }
            }
            break;
        default:
            printf("other event type[%d]\n", dwAlarmType);
            break;
    }

    return 1;
}
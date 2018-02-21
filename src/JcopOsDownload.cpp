/******************************************************************************
 *
 *  Copyright 2018 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#include <android-base/stringprintf.h>
#include <base/logging.h>
#include <semaphore.h>
#include <JcopOsDownload.h>
#include <IChannel.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

using android::base::StringPrintf;
JcopOsDwnld JcopOsDwnld::sJcopDwnld;
int32_t gTransceiveTimeout = 120000;
uint8_t isUaiEnabled = false;

tJBL_STATUS (JcopOsDwnld::*JcopOs_dwnld_seqhandler[])(
            JcopOs_ImageInfo_t* pContext, tJBL_STATUS status, JcopOs_TranscieveInfo_t* pInfo)={
       &JcopOsDwnld::TriggerApdu,
       &JcopOsDwnld::GetInfo,
       &JcopOsDwnld::load_JcopOS_image,
       &JcopOsDwnld::GetInfo,
       &JcopOsDwnld::load_JcopOS_image,
       &JcopOsDwnld::GetInfo,
       &JcopOsDwnld::load_JcopOS_image,
       NULL
   };

pJcopOs_Dwnld_Context_t gpJcopOs_Dwnld_Context = NULL;
static const char *path[3] = {"/data/vendor/nfc/JcopOs_Update1.apdu",
                             "/data/vendor/nfc/JcopOs_Update2.apdu",
                             "/data/vendor/nfc/JcopOs_Update3.apdu"};

static const char *uai_path[2] = {"/data/vendor/nfc/cci.jcsh",
                                  "/data/vendor/nfc/jci.jcsh"};

inline int FSCANF_BYTE(FILE *stream, const char *format, void* pVal)
{
    int Result = 0;

    if((NULL != stream) && (NULL != format) && (NULL != pVal))
    {
        unsigned int dwVal;
        unsigned char* pTmp = (unsigned char*)pVal;
        Result = fscanf(stream, format, &dwVal);

        (*pTmp) = (unsigned char)(dwVal & 0x000000FF);
    }
    return Result;
}

/*******************************************************************************
**
** Function:        getInstance
**
** Description:     Get the JcopOsDwnld singleton object.
**
** Returns:         JcopOsDwnld object.
**
*******************************************************************************/
JcopOsDwnld* JcopOsDwnld::getInstance()
{
    JcopOsDwnld *jd = new JcopOsDwnld();
    return jd;
}

/*******************************************************************************
**
** Function:        getJcopOsFileInfo
**
** Description:     Verify all the updater files required for download
**                  are present or not
**
** Returns:         True if ok.
**
*******************************************************************************/
bool JcopOsDwnld::getJcopOsFileInfo()
{
    static const char fn [] = "JcopOsDwnld::getJcopOsFileInfo";
    bool status = true;
    struct stat st;
    LOG(INFO) << StringPrintf("%s: Enter", fn);
    for (int num = 0; num < 2; num++)
    {
        if (stat(uai_path[num], &st))
        {
            status = false;
        }
    }
    /*If UAI specific files are present*/
    if(status == true)
    {
        isUaiEnabled = true;
    }
    for (int num = 0; num < 3; num++)
    {
        if (stat(path[num], &st))
        {
            status = false;
        }
    }
    LOG(INFO) << StringPrintf("%s: Exit Status %d", fn, status);
    return status;
}

/*******************************************************************************
**
** Function:        initialize
**
** Description:     Initialize all member variables.
**                  native: Native data.
**
** Returns:         True if ok.
**
*******************************************************************************/
bool JcopOsDwnld::initialize (IChannel_t *channel)
{
    static const char fn [] = "JcopOsDwnld::initialize";
    isUaiEnabled = false;
    LOG(INFO) << StringPrintf("%s: enter", fn);

    if (!getJcopOsFileInfo())
    {
        LOG(ERROR)<< StringPrintf(
            "%s: insufficient resources, file not present", fn);
        return (false);
    }
    gpJcopOs_Dwnld_Context = (pJcopOs_Dwnld_Context_t)malloc(sizeof(JcopOs_Dwnld_Context_t));
    if(gpJcopOs_Dwnld_Context != NULL)
    {
        memset((void *)gpJcopOs_Dwnld_Context, 0, (uint32_t)sizeof(JcopOs_Dwnld_Context_t));
        gpJcopOs_Dwnld_Context->channel = (IChannel_t*)malloc(sizeof(IChannel_t));
        if(gpJcopOs_Dwnld_Context->channel != NULL)
        {
            memset(gpJcopOs_Dwnld_Context->channel, 0, sizeof(IChannel_t));
        }
        else
        {
            LOG(ERROR)<< StringPrintf(
                "%s: Memory allocation for IChannel is failed", fn);
            return (false);
        }
        gpJcopOs_Dwnld_Context->pJcopOs_TransInfo.sSendData = (uint8_t*)malloc(sizeof(uint8_t)*JCOP_MAX_BUF_SIZE);
        if(gpJcopOs_Dwnld_Context->pJcopOs_TransInfo.sSendData != NULL)
        {
            memset(gpJcopOs_Dwnld_Context->pJcopOs_TransInfo.sSendData, 0, JCOP_MAX_BUF_SIZE);
        }
        else
        {
            LOG(ERROR)<< StringPrintf(
                "%s: Memory allocation for SendBuf is failed", fn);
            return (false);
        }
    }
    else
    {
        LOG(ERROR)<< StringPrintf(
            "%s: Memory allocation failed", fn);
        return (false);
    }
    mIsInit = true;
    memcpy(gpJcopOs_Dwnld_Context->channel, channel, sizeof(IChannel_t));
    LOG(INFO) << StringPrintf("%s: exit", fn);
    return (true);
}
/*******************************************************************************
**
** Function:        finalize
**
** Description:     Release all resources.
**
** Returns:         None
**
*******************************************************************************/
void JcopOsDwnld::finalize ()
{
    static const char fn [] = "JcopOsDwnld::finalize";
    LOG(INFO) << StringPrintf("%s: enter", fn);
    mIsInit       = false;
    if(gpJcopOs_Dwnld_Context != NULL)
    {
        if(gpJcopOs_Dwnld_Context->channel != NULL)
        {
            free(gpJcopOs_Dwnld_Context->channel);
            gpJcopOs_Dwnld_Context->channel = NULL;
        }
        if(gpJcopOs_Dwnld_Context->pJcopOs_TransInfo.sSendData != NULL)
        {
            free(gpJcopOs_Dwnld_Context->pJcopOs_TransInfo.sSendData);
            gpJcopOs_Dwnld_Context->pJcopOs_TransInfo.sSendData = NULL;
        }
        free(gpJcopOs_Dwnld_Context);
        gpJcopOs_Dwnld_Context = NULL;
    }
    LOG(INFO) << StringPrintf("%s: exit", fn);
}

/*******************************************************************************
**
** Function:        JcopOs_Download
**
** Description:     Starts the OS download sequence
**
** Returns:         Success if ok.
**
*******************************************************************************/
tJBL_STATUS JcopOsDwnld::JcopOs_Download()
{
    static const char fn [] = "JcopOsDwnld::JcopOs_Download";
    tJBL_STATUS wstatus = STATUS_FAILED;
    uint8_t retry_cnt = 0x00;
    LOG(INFO) << StringPrintf("%s: Enter:", fn);
    if(mIsInit == false)
    {
        LOG(ERROR)<< StringPrintf (
            "%s: JcopOs Dwnld is not initialized", fn);
        wstatus = STATUS_FAILED;
    }
    else
    {
        do
        {
            wstatus = JcopOsDwnld::JcopOs_update_seq_handler();
            if(wstatus == STATUS_FAILED)
                retry_cnt++;
            else
                break;
        }while(retry_cnt < JCOP_MAX_RETRY_CNT);
    }
    LOG(INFO) << StringPrintf("%s: Exit; status = 0x%x", fn, wstatus);
    return wstatus;
}
/*******************************************************************************
**
** Function:        JcopOs_update_seq_handler
**
** Description:     Performs the JcopOS download sequence
**
** Returns:         Success if ok.
**
*******************************************************************************/
tJBL_STATUS JcopOsDwnld::JcopOs_update_seq_handler()
{
    static const char fn[] = "JcopOsDwnld::JcopOs_update_seq_handler";
    uint8_t seq_counter = 0;
    JcopOs_ImageInfo_t update_info = (JcopOs_ImageInfo_t )gpJcopOs_Dwnld_Context->Image_info;
    JcopOs_TranscieveInfo_t trans_info = (JcopOs_TranscieveInfo_t )gpJcopOs_Dwnld_Context->pJcopOs_TransInfo;
    update_info.index = 0x00;
    update_info.cur_state = 0x00;
    tJBL_STATUS status = STATUS_FAILED;

    LOG(INFO) << StringPrintf("%s: enter", fn);
    status = GetJcopOsState(&update_info, &seq_counter);
    if(status != STATUS_SUCCESS)
    {
        LOG(ERROR) << StringPrintf("Error in getting JcopOsState info");
    }
    else
    {
        LOG(INFO) << StringPrintf("seq_counter %d", seq_counter);
        while((JcopOs_dwnld_seqhandler[seq_counter]) != NULL )
        {
            status = STATUS_FAILED;
            status = (*this.*(JcopOs_dwnld_seqhandler[seq_counter]))(&update_info, status, &trans_info );
            if(STATUS_SUCCESS != status)
            {
                LOG(ERROR) << StringPrintf("%s: exiting; status=0x0%X", fn, status);
                break;
            }
            seq_counter++;
        }
    }
    return status;
}

/*******************************************************************************
**
** Function:        TriggerApdu
**
** Description:     Switch to updater OS
**
** Returns:         Success if ok.
**
*******************************************************************************/
tJBL_STATUS JcopOsDwnld::TriggerApdu(JcopOs_ImageInfo_t* pVersionInfo, tJBL_STATUS status, JcopOs_TranscieveInfo_t* pTranscv_Info)
{
    static const char fn [] = "JcopOsDwnld::TriggerApdu";
    bool stat = false;
    IChannel_t *mchannel = gpJcopOs_Dwnld_Context->channel;
    int32_t recvBufferActualSize = 0;
    LOG(INFO) << StringPrintf("%s: enter;", fn);
    if(isUaiEnabled)
    {
        mchannel->doeSE_JcopDownLoadReset();
        stat = UaiTriggerApdu(pVersionInfo, status, pTranscv_Info);
        if(stat == STATUS_OK)
        {
            stat = SendUAICmds(pVersionInfo, status, pTranscv_Info);
        }
        if(stat != STATUS_OK)
        {
            LOG(ERROR)<< StringPrintf(
                "%s: exit; status = 0x%X", fn, status);
            status = STATUS_FAILED;
            return status;
        }
    }
    if(pTranscv_Info == NULL ||
       pVersionInfo == NULL)
    {
        LOG(ERROR)<< StringPrintf("%s: Invalid parameter", fn);
        status = STATUS_FAILED;
    }
    else
    {
        pTranscv_Info->timeout = gTransceiveTimeout;
        pTranscv_Info->sSendlength = (int32_t)sizeof(Trigger_APDU);
        pTranscv_Info->sRecvlength = 1024;//(int32_t)sizeof(int32_t);
        memcpy(pTranscv_Info->sSendData, Trigger_APDU, pTranscv_Info->sSendlength);

        LOG(INFO) << StringPrintf("%s: Calling Secure Element Transceive", fn);
        stat = mchannel->transceive (pTranscv_Info->sSendData,
                                pTranscv_Info->sSendlength,
                                pTranscv_Info->sRecvData,
                                pTranscv_Info->sRecvlength,
                                recvBufferActualSize,
                                pTranscv_Info->timeout);
        if (stat != true)
        {
            status = STATUS_FAILED;
            LOG(ERROR) << StringPrintf(
                "%s: SE transceive failed status = 0x%X", fn, status);//Stop JcopOs Update
        }
        else if(((pTranscv_Info->sRecvData[recvBufferActualSize-2] == 0x68) &&
               (pTranscv_Info->sRecvData[recvBufferActualSize-1] == 0x81))||
               ((pTranscv_Info->sRecvData[recvBufferActualSize-2] == 0x90) &&
               (pTranscv_Info->sRecvData[recvBufferActualSize-1] == 0x00))||
               ((pTranscv_Info->sRecvData[recvBufferActualSize-2] == 0x6F) &&
               (pTranscv_Info->sRecvData[recvBufferActualSize-1] == 0x00)))
        {
            mchannel->doeSE_JcopDownLoadReset();
            status = STATUS_OK;
            LOG(INFO) << StringPrintf("%s: Trigger APDU Transceive status = 0x%X", fn, status);
        }
        else
        {
            /* status {90, 00} */
            status = STATUS_OK;
        }
    }
    LOG(INFO) << StringPrintf("%s: exit; status = 0x%X", fn, status);
    return status;
}

/*******************************************************************************
**
** Function:        SendUAICmds
**
** Description:     Perform UAI Authentication
**
** Returns:         Success if ok.
**
*******************************************************************************/
tJBL_STATUS JcopOsDwnld::SendUAICmds(JcopOs_ImageInfo_t* Os_info, tJBL_STATUS status, JcopOs_TranscieveInfo_t* pTranscv_Info)
{
    static const char fn [] = "JcopOsDwnld::SendUAICmds";
    bool stat = false;
    int wResult;
    int32_t wIndex,wCount=0;
    int32_t wLen;
    IChannel_t *mchannel = gpJcopOs_Dwnld_Context->channel;
    int32_t recvBufferActualSize = 0;
    int i = 0;

    LOG(INFO) << StringPrintf("%s: enter;", fn);

    if(pTranscv_Info == NULL ||
               Os_info == NULL)
    {
        LOG(ERROR)<< StringPrintf("%s: Invalid parameter", fn);
        status = STATUS_FAILED;
    }
    for(i = 0; i < 2; i++)
    {
        Os_info->fp = fopen(uai_path[i], "r+");
        if (Os_info->fp == NULL) {
            LOG(ERROR) << StringPrintf("Error opening CCI file <%s> for reading: %s",
                        Os_info->fls_path, strerror(errno));
            return STATUS_FILE_NOT_FOUND;
        }
        wResult = fseek(Os_info->fp, 0L, SEEK_END);
        if (wResult) {
            LOG(ERROR) << StringPrintf("Error seeking end CCI file %s", strerror(errno));
            goto exit;
        }
        Os_info->fls_size = ftell(Os_info->fp);
        if (Os_info->fls_size < 0) {
            LOG(ERROR) << StringPrintf("Error ftelling file %s", strerror(errno));
            goto exit;
        }
        wResult = fseek(Os_info->fp, 6L, SEEK_SET);
        if (wResult) {
            LOG(ERROR) << StringPrintf("Error seeking start image file %s", strerror(errno));
            goto exit;
        }
        {
            wIndex=0;
            wLen=0;
            wCount=0;
            memset(pTranscv_Info->sSendData,0x00,JCOP_MAX_BUF_SIZE);
            pTranscv_Info->sSendlength=0;

            LOG(INFO) << StringPrintf("%s; wIndex = 0", fn);
            for(wCount =0; (wCount < 5 && !feof(Os_info->fp)); wCount++, wIndex++)
            {
                wResult = FSCANF_BYTE(Os_info->fp,"%2X",&pTranscv_Info->sSendData[wIndex]);
            }
            if(wResult != 0)
            {
                wLen = pTranscv_Info->sSendData[4];
                LOG(INFO) << StringPrintf("%s; Read 5byes success & len=%d", fn,wLen);
                if(wLen == 0x00)
                {
                    LOG(INFO) << StringPrintf("%s: Extended APDU", fn);
                    wResult = FSCANF_BYTE(Os_info->fp,"%2X",&pTranscv_Info->sSendData[wIndex++]);
                    wResult = FSCANF_BYTE(Os_info->fp,"%2X",&pTranscv_Info->sSendData[wIndex++]);
                    wLen = ((pTranscv_Info->sSendData[5] << 8) | (pTranscv_Info->sSendData[6]));
                }
                for(wCount =0; (wCount < wLen && !feof(Os_info->fp)); wCount++, wIndex++)
                {
                    wResult = FSCANF_BYTE(Os_info->fp,"%2X",&pTranscv_Info->sSendData[wIndex]);
                }
            }
            else
            {
                LOG(ERROR) << StringPrintf("%s: JcopOs image Read failed", fn);
                goto exit;
            }
            pTranscv_Info->sSendlength = wIndex;
            LOG(INFO) << StringPrintf(
                "%s: start transceive for length %d", fn, pTranscv_Info->sSendlength);
            if((pTranscv_Info->sSendlength != 0x03) &&
               (pTranscv_Info->sSendData[0] != 0x00) &&
               (pTranscv_Info->sSendData[1] != 0x00))
            {

                stat = mchannel->transceive(pTranscv_Info->sSendData,
                                        pTranscv_Info->sSendlength,
                                        pTranscv_Info->sRecvData,
                                        pTranscv_Info->sRecvlength,
                                        recvBufferActualSize,
                                        pTranscv_Info->timeout);
            }
            else
            {
                LOG(ERROR) << StringPrintf("%s: Invalid packet", fn);
                goto exit;
            }
            if(stat != true)
            {
                LOG(ERROR) << StringPrintf("%s: Transceive failed; status=0x%X", fn, stat);
                status = STATUS_FAILED;
                goto exit;
            }
            else if(recvBufferActualSize != 0 &&
                    pTranscv_Info->sRecvData[recvBufferActualSize-2] == 0x90 &&
                    pTranscv_Info->sRecvData[recvBufferActualSize-1] == 0x00)
            {
                status = STATUS_SUCCESS;
            }
            else if(pTranscv_Info->sRecvData[recvBufferActualSize-2] == 0x6F &&
                    pTranscv_Info->sRecvData[recvBufferActualSize-1] == 0x00)
            {
                LOG(INFO) << StringPrintf(
                    "%s: JcopOs is already upto date-No update required exiting", fn);
                Os_info->version_info.ver_status = STATUS_UPTO_DATE;
                status = STATUS_FAILED;
                break;
            }
            else
            {
                status = STATUS_FAILED;
                LOG(ERROR) << StringPrintf("%s: Invalid response", fn);
                goto exit;
            }
        }
        wResult = fclose(Os_info->fp);
    }
exit:
    LOG(INFO) << StringPrintf("%s close fp and exit; status= 0x%X", fn,status);
    mchannel->doeSE_JcopDownLoadReset();
    if(status == STATUS_FAILED)
        wResult = fclose(Os_info->fp);
    return status;
}
/*******************************************************************************
**
** Function:        UaiTriggerApdu
**
** Description:     Switch to updater OS
**
** Returns:         Success if ok.
**
*******************************************************************************/
tJBL_STATUS JcopOsDwnld::UaiTriggerApdu(JcopOs_ImageInfo_t* pVersionInfo, tJBL_STATUS status, JcopOs_TranscieveInfo_t* pTranscv_Info)
{
    static const char fn [] = "JcopOsDwnld::UaiTriggerApdu";
    bool stat = false;
    IChannel_t *mchannel = gpJcopOs_Dwnld_Context->channel;
    int32_t recvBufferActualSize = 0;

    LOG(INFO) << StringPrintf("%s: enter;", fn);
    if(pTranscv_Info == NULL ||
       pVersionInfo == NULL)
    {
        LOG(ERROR)<< StringPrintf("%s: Invalid parameter", fn);
        status = STATUS_FAILED;
    }
    else
    {
        pTranscv_Info->timeout = gTransceiveTimeout;
        pTranscv_Info->sSendlength = (int32_t)sizeof(Uai_Trigger_APDU);
        pTranscv_Info->sRecvlength = 1024;//(int32_t)sizeof(int32_t);
        memcpy(pTranscv_Info->sSendData, Uai_Trigger_APDU, pTranscv_Info->sSendlength);

        LOG(INFO) << StringPrintf("%s: Calling Secure Element Transceive", fn);
        stat = mchannel->transceive (pTranscv_Info->sSendData,
                                pTranscv_Info->sSendlength,
                                pTranscv_Info->sRecvData,
                                pTranscv_Info->sRecvlength,
                                recvBufferActualSize,
                                pTranscv_Info->timeout);
        if (stat != true)
        {
            status = STATUS_FAILED;
            LOG(ERROR) << StringPrintf(
                "%s: SE transceive failed status = 0x%X", fn, status);//Stop JcopOs Update
        }
        else if(((pTranscv_Info->sRecvData[recvBufferActualSize-2] == 0x68) &&
               (pTranscv_Info->sRecvData[recvBufferActualSize-1] == 0x81))||
               ((pTranscv_Info->sRecvData[recvBufferActualSize-2] == 0x90) &&
               (pTranscv_Info->sRecvData[recvBufferActualSize-1] == 0x00))||
               ((pTranscv_Info->sRecvData[recvBufferActualSize-2] == 0x6F) &&
               (pTranscv_Info->sRecvData[recvBufferActualSize-1] == 0x00)))
        {
            /*mchannel->doeSE_JcopDownLoadReset();*/
            status = STATUS_OK;
            LOG(INFO) << StringPrintf("%s: Trigger APDU Transceive status = 0x%X", fn, status);
        }
        else
        {
            /* status {90, 00} */
            status = STATUS_OK;
        }
    }
    LOG(INFO) << StringPrintf("%s: exit; status = 0x%X", fn, status);
    return status;
}
/*******************************************************************************
**
** Function:        GetInfo
**
** Description:     Get the JCOP OS info
**
** Returns:         Success if ok.
**
*******************************************************************************/
tJBL_STATUS JcopOsDwnld::GetInfo(JcopOs_ImageInfo_t* pImageInfo, tJBL_STATUS status, JcopOs_TranscieveInfo_t* pTranscv_Info)
{
    static const char fn [] = "JcopOsDwnld::GetInfo";

    bool stat = false;
    IChannel_t *mchannel = gpJcopOs_Dwnld_Context->channel;
    int32_t recvBufferActualSize = 0;

    LOG(INFO) << StringPrintf("%s: enter;", fn);

    if(pTranscv_Info == NULL ||
       pImageInfo == NULL)
    {
        LOG(ERROR)<< StringPrintf("%s: Invalid parameter", fn);
        status = STATUS_FAILED;
    }
    else
    {
        memcpy(pImageInfo->fls_path, (char *)path[pImageInfo->index], strlen(path[pImageInfo->index]));
        memset(pTranscv_Info->sSendData, 0, JCOP_MAX_BUF_SIZE);
        pTranscv_Info->timeout = gTransceiveTimeout;
        if(isUaiEnabled)
        {
             pTranscv_Info->sSendlength = (uint32_t)sizeof(Uai_GetInfo_APDU);
             memcpy(pTranscv_Info->sSendData, Uai_GetInfo_APDU, pTranscv_Info->sSendlength);
        }else
        {
            pTranscv_Info->sSendlength = (uint32_t)sizeof(GetInfo_APDU);
            memcpy(pTranscv_Info->sSendData, GetInfo_APDU, pTranscv_Info->sSendlength);
        }
        pTranscv_Info->sRecvlength = 1024;

        LOG(INFO) << StringPrintf("%s: Calling Secure Element Transceive", fn);
        stat = mchannel->transceive (pTranscv_Info->sSendData,
                                pTranscv_Info->sSendlength,
                                pTranscv_Info->sRecvData,
                                pTranscv_Info->sRecvlength,
                                recvBufferActualSize,
                                pTranscv_Info->timeout);
        if (stat != true)
        {
            status = STATUS_FAILED;
            pImageInfo->index =0;
            LOG(ERROR) << StringPrintf("%s: SE transceive failed status = 0x%X", fn, status);//Stop JcopOs Update
        }
        else if((pTranscv_Info->sRecvData[recvBufferActualSize-2] == 0x90) &&
                (pTranscv_Info->sRecvData[recvBufferActualSize-1] == 0x00))
        {
            pImageInfo->version_info.osid = pTranscv_Info->sRecvData[recvBufferActualSize-6];
            pImageInfo->version_info.ver1 = pTranscv_Info->sRecvData[recvBufferActualSize-5];
            pImageInfo->version_info.ver0 = pTranscv_Info->sRecvData[recvBufferActualSize-4];
            pImageInfo->version_info.OtherValid = pTranscv_Info->sRecvData[recvBufferActualSize-3];
            {
                LOG(ERROR) << StringPrintf("Starting 3-Step update");
                memcpy(pImageInfo->fls_path, path[pImageInfo->index], sizeof(path[pImageInfo->index]));
                pImageInfo->index++;
            }
            status = STATUS_OK;
            LOG(INFO) << StringPrintf("%s: GetInfo Transceive status = 0x%X", fn, status);
        }
        else if((pTranscv_Info->sRecvData[recvBufferActualSize-2] == 0x6A) &&
                (pTranscv_Info->sRecvData[recvBufferActualSize-1] == 0x82) &&
                 pImageInfo->version_info.ver_status == STATUS_UPTO_DATE)
        {
            status = STATUS_UPTO_DATE;
        }
        else
        {
            status = STATUS_FAILED;
            LOG(ERROR)<< StringPrintf("%s; Invalid response for GetInfo", fn);
        }
    }

    if (status == STATUS_FAILED)
    {
        LOG(ERROR)<< StringPrintf("%s; status failed, doing reset...", fn);
        mchannel->doeSE_JcopDownLoadReset();
    }
    LOG(INFO) << StringPrintf("%s: exit; status = 0x%X", fn, status);
    return status;
}
/*******************************************************************************
**
** Function:        load_JcopOS_image
**
** Description:     Used to update the JCOP OS
**                  Get Info function has to be called before this
**
** Returns:         Success if ok.
**
*******************************************************************************/
tJBL_STATUS JcopOsDwnld::load_JcopOS_image(JcopOs_ImageInfo_t *Os_info, tJBL_STATUS status, JcopOs_TranscieveInfo_t *pTranscv_Info)
{
    static const char fn [] = "JcopOsDwnld::load_JcopOS_image";
    bool stat = false;
    int wResult;
    int32_t wIndex,wCount=0;
    int32_t wLen;

    IChannel_t *mchannel = gpJcopOs_Dwnld_Context->channel;
    int32_t recvBufferActualSize = 0;
    LOG(ERROR)
      << StringPrintf("%s: enter", fn);
    if(Os_info == NULL ||
       pTranscv_Info == NULL)
    {
        LOG(ERROR) << StringPrintf("%s: invalid parameter", fn);
        return status;
    }
    Os_info->fp = fopen(Os_info->fls_path, "r+");

    if (Os_info->fp == NULL) {
        LOG(ERROR) << StringPrintf("Error opening OS image file <%s> for reading: %s",
                    Os_info->fls_path, strerror(errno));
        return STATUS_FILE_NOT_FOUND;
    }
    wResult = fseek(Os_info->fp, 0L, SEEK_END);
    if (wResult) {
        LOG(ERROR) << StringPrintf("Error seeking end OS image file %s", strerror(errno));
        goto exit;
    }
    Os_info->fls_size = ftell(Os_info->fp);
    if (Os_info->fls_size < 0) {
        LOG(ERROR) << StringPrintf("Error ftelling file %s", strerror(errno));
        goto exit;
    }
    wResult = fseek(Os_info->fp, 0L, SEEK_SET);
    if (wResult) {
        LOG(ERROR) << StringPrintf("Error seeking start image file %s", strerror(errno));
        goto exit;
    }
    while(!feof(Os_info->fp))
    {
        LOG(INFO) << StringPrintf("%s; Start of line processing", fn);

        wIndex=0;
        wLen=0;
        wCount=0;
        memset(pTranscv_Info->sSendData,0x00,JCOP_MAX_BUF_SIZE);
        pTranscv_Info->sSendlength=0;

        LOG(INFO) << StringPrintf("%s; wIndex = 0", fn);
        for(wCount =0; (wCount < 5 && !feof(Os_info->fp)); wCount++, wIndex++)
        {
            wResult = FSCANF_BYTE(Os_info->fp,"%2X",&pTranscv_Info->sSendData[wIndex]);
        }
        if(wResult != 0)
        {
            wLen = pTranscv_Info->sSendData[4];
            LOG(INFO) << StringPrintf("%s; Read 5byes success & len=%d", fn,wLen);
            if(wLen == 0x00)
            {
                LOG(INFO) << StringPrintf("%s: Extended APDU", fn);
                wResult = FSCANF_BYTE(Os_info->fp,"%2X",&pTranscv_Info->sSendData[wIndex++]);
                wResult = FSCANF_BYTE(Os_info->fp,"%2X",&pTranscv_Info->sSendData[wIndex++]);
                wLen = ((pTranscv_Info->sSendData[5] << 8) | (pTranscv_Info->sSendData[6]));
            }
            for(wCount =0; (wCount < wLen && !feof(Os_info->fp)); wCount++, wIndex++)
            {
                wResult = FSCANF_BYTE(Os_info->fp,"%2X",&pTranscv_Info->sSendData[wIndex]);
            }
        }
        else
        {
            LOG(ERROR) << StringPrintf("%s: JcopOs image Read failed", fn);
            goto exit;
        }

        pTranscv_Info->sSendlength = wIndex;
        LOG(INFO) << StringPrintf("%s: start transceive for length %d", fn, pTranscv_Info->sSendlength);
        if((pTranscv_Info->sSendlength != 0x03) &&
           (pTranscv_Info->sSendData[0] != 0x00) &&
           (pTranscv_Info->sSendData[1] != 0x00))
        {

            stat = mchannel->transceive(pTranscv_Info->sSendData,
                                    pTranscv_Info->sSendlength,
                                    pTranscv_Info->sRecvData,
                                    pTranscv_Info->sRecvlength,
                                    recvBufferActualSize,
                                    pTranscv_Info->timeout);
        }
        else
        {
            LOG(ERROR) << StringPrintf("%s: Invalid packet", fn);
            continue;
        }
        if(stat != true)
        {
            LOG(ERROR) << StringPrintf("%s: Transceive failed; status=0x%X", fn, stat);
            status = STATUS_FAILED;
            goto exit;
        }
        else if(recvBufferActualSize != 0 &&
                pTranscv_Info->sRecvData[recvBufferActualSize-2] == 0x90 &&
                pTranscv_Info->sRecvData[recvBufferActualSize-1] == 0x00)
        {
            //LOG(ERROR) << StringPrintf("%s: END transceive for length %d", fn, pTranscv_Info->sSendlength);
            status = STATUS_SUCCESS;
        }
        else if(pTranscv_Info->sRecvData[recvBufferActualSize-2] == 0x6F &&
                pTranscv_Info->sRecvData[recvBufferActualSize-1] == 0x00)
        {
            LOG(ERROR) << StringPrintf("%s: JcopOs is already upto date-No update required exiting", fn);
            Os_info->version_info.ver_status = STATUS_UPTO_DATE;
            status = STATUS_FAILED;
            break;
        }
        else
        {
            status = STATUS_FAILED;
            LOG(ERROR) << StringPrintf("%s: Invalid response", fn);
            goto exit;
        }
        LOG(INFO) << StringPrintf("%s: Going for next line", fn);
    }

    if(status == STATUS_SUCCESS)
    {
        Os_info->cur_state++;
        SetJcopOsState(Os_info, Os_info->cur_state);
    }

exit:
    mchannel->doeSE_JcopDownLoadReset();
    LOG(INFO) << StringPrintf("%s close fp and exit; status= 0x%X", fn,status);
    wResult = fclose(Os_info->fp);
    return status;
}

/*******************************************************************************
**
** Function:        GetJcopOsState
**
** Description:     Used to update the JCOP OS state
**
** Returns:         Success if ok.
**
*******************************************************************************/
tJBL_STATUS JcopOsDwnld::GetJcopOsState(JcopOs_ImageInfo_t *Os_info, uint8_t *counter)
{
    static const char fn [] = "JcopOsDwnld::GetJcopOsState";
    tJBL_STATUS status = STATUS_SUCCESS;
    FILE *fp;
    uint8_t xx=0;
    LOG(INFO) << StringPrintf("%s: enter", fn);
    if(Os_info == NULL)
    {
        LOG(ERROR) << StringPrintf("%s: invalid parameter", fn);
        return STATUS_FAILED;
    }
    fp = fopen(JCOP_INFO_PATH, "r");

    if (fp == NULL) {
        LOG(ERROR) << StringPrintf("file <%s> not exits for reading- creating new file: %s",
                JCOP_INFO_PATH, strerror(errno));
        fp = fopen(JCOP_INFO_PATH, "w+");
        if (fp == NULL)
        {
            LOG(ERROR) << StringPrintf("Error opening OS image file <%s> for reading: %s",
                    JCOP_INFO_PATH, strerror(errno));
            return STATUS_FAILED;
        }
        fprintf(fp, "%u", xx);
        fclose(fp);
    }
    else
    {
        FSCANF_BYTE(fp, "%u", &xx);
        LOG(INFO) << StringPrintf("JcopOsState %d", xx);
        fclose(fp);
    }

    switch(xx)
    {
    case JCOP_UPDATE_STATE0:
    case JCOP_UPDATE_STATE3:
        LOG(INFO) << StringPrintf("Starting update from step1");
        Os_info->index = JCOP_UPDATE_STATE0;
        Os_info->cur_state = JCOP_UPDATE_STATE0;
        *counter = 0;
        break;
    case JCOP_UPDATE_STATE1:
        LOG(INFO) << StringPrintf("Starting update from step2");
        Os_info->index = JCOP_UPDATE_STATE1;
        Os_info->cur_state = JCOP_UPDATE_STATE1;
        *counter = 3;
        break;
    case JCOP_UPDATE_STATE2:
        LOG(INFO) << StringPrintf("Starting update from step3");
        Os_info->index = JCOP_UPDATE_STATE2;
        Os_info->cur_state = JCOP_UPDATE_STATE2;
        *counter = 5;
        break;
    default:
        LOG(INFO) << StringPrintf("invalid state");
        status = STATUS_FAILED;
        break;
    }
    return status;
}

/*******************************************************************************
**
** Function:        SetJcopOsState
**
** Description:     Used to set the JCOP OS state
**
** Returns:         Success if ok.
**
*******************************************************************************/
tJBL_STATUS JcopOsDwnld::SetJcopOsState(JcopOs_ImageInfo_t *Os_info, uint8_t state)
{
    static const char fn [] = "JcopOsDwnld::SetJcopOsState";
    tJBL_STATUS status = STATUS_FAILED;
    FILE *fp;
    LOG(INFO) << StringPrintf("%s: enter", fn);
    if(Os_info == NULL)
    {
        LOG(ERROR) << StringPrintf("%s: invalid parameter", fn);
        return status;
    }
    fp = fopen(JCOP_INFO_PATH, "w");

    if (fp == NULL) {
        LOG(ERROR) << StringPrintf("Error opening OS image file <%s> for reading: %s",
                JCOP_INFO_PATH, strerror(errno));
    }
    else
    {
        int fd=fileno(fp);
        int ret = fdatasync(fd);
        fprintf(fp, "%u", state);
        fflush(fp);
        LOG(INFO) << StringPrintf("Current JcopOsState: %d", state);
        status = STATUS_SUCCESS;
        LOG(INFO) << StringPrintf("ret value: %d", ret);
        fclose(fp);
    }
    return status;
}

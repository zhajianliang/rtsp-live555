
#include <iostream>

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
// 定义一个结构体来保存文件状态
typedef struct
{
    FILE *file;
    int end_of_file; // 标记是否到达文件末尾
} FrameReader;

// 初始化函数，打开文件并设置初始状态
int init_frame_reader(const char *filename, FrameReader *reader)
{
    reader->file = fopen(filename, "r");
    if (reader->file == NULL)
    {
        perror("Error opening file");
        return -1;
    }
    reader->end_of_file = 0;
    return 0;
}

// 关闭文件并清理资源
void close_frame_reader(FrameReader *reader)
{
    if (reader->file != NULL)
    {
        fclose(reader->file);
        reader->file = NULL;
    }
}

// 逐帧读取帧长度
int read_next_frame_length(FrameReader *reader, int *frame_length)
{
    if (reader->end_of_file || reader->file == NULL)
    {
        return -1; // 文件已经结束或者未正确初始化
    }

    // 尝试读取一个整数作为帧长度
    if (fscanf(reader->file, "%d", frame_length) != 1)
    {
        // 如果读取失败，检查是否是因为到达文件末尾
        if (feof(reader->file))
        {
            reader->end_of_file = 1;
            return 0; // 返回 0 表示没有更多帧可读
        }
        else
        {
            perror("Error reading frame length");
            return -1; // 其他错误
        }
    }

    // 跳过剩余的行内容（如果有）
    int c;
    while ((c = fgetc(reader->file)) != '\n' && c != EOF)
        ;

    return 1; // 成功读取一帧
}
void create_rtsp_server(void);

class H264LiveServerMediaSession : public OnDemandServerMediaSubsession
{
public:
    static H264LiveServerMediaSession *createNew(UsageEnvironment &env, Boolean reuseFirstSource);
    void checkForAuxSDPLine1();
    void afterPlayingDummy1();

protected:
    H264LiveServerMediaSession(UsageEnvironment &env, Boolean reuseFirstSource);
    virtual ~H264LiveServerMediaSession(void);
    void setDoneFlag() { fDoneFlag = ~0; }

protected:
    virtual char const *getAuxSDPLine(RTPSink *rtpSink, FramedSource *inputSource);
    virtual FramedSource *createNewStreamSource(unsigned clientSessionId, unsigned &estBitrate);
    virtual RTPSink *createNewRTPSink(Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource *inputSource);

private:
    char *fAuxSDPLine;
    char fDoneFlag;
    RTPSink *fDummyRTPSink;
};

// 创建一个自定义的实时码流数据源类
class H264VideoStreamSource : public FramedSource
{
public:
    static H264VideoStreamSource *createNew(UsageEnvironment &env);
    unsigned maxFrameSize() const;

protected:
    H264VideoStreamSource(UsageEnvironment &env);
    virtual ~H264VideoStreamSource();

private:
    virtual void doGetNextFrame();
    virtual void doStopGettingFrames();
};
void create_rtsp_server(void)
{
    TaskScheduler *scheduler;
    UsageEnvironment *env;
    RTSPServer *rtspServer;

    scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);
    rtspServer = RTSPServer::createNew(*env, 8554);
    if (rtspServer == NULL)
    {
        *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
        return;
    }

    ServerMediaSession *sms = ServerMediaSession::createNew(*env);
    sms->addSubsession(H264LiveServerMediaSession::createNew(*env, true));
    rtspServer->addServerMediaSession(sms);
    char *url = rtspServer->rtspURL(sms);
    *env << "Play the stream using url " << url << "\n";
    delete[] url;
    env->taskScheduler().doEventLoop(); // 进入事件循环
}

// H264LiveServerMediaSession 实现：
H264LiveServerMediaSession *H264LiveServerMediaSession::createNew(UsageEnvironment &env, Boolean reuseFirstSource)
{
    return new H264LiveServerMediaSession(env, reuseFirstSource);
}

H264LiveServerMediaSession::H264LiveServerMediaSession(UsageEnvironment &env, Boolean reuseFirstSource) : OnDemandServerMediaSubsession(env, reuseFirstSource)
{
    fAuxSDPLine = NULL;
    fDoneFlag = 0;
    fDummyRTPSink = NULL;
}

H264LiveServerMediaSession::~H264LiveServerMediaSession()
{
    delete[] fAuxSDPLine;
}

static void afterPlayingDummy(void *clientData)
{
    H264LiveServerMediaSession *subsess = (H264LiveServerMediaSession *)clientData;
    subsess->afterPlayingDummy1();
}

void H264LiveServerMediaSession::afterPlayingDummy1()
{
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
    setDoneFlag();
}

static void checkForAuxSDPLine(void *clientData)
{
    H264LiveServerMediaSession *subsess = (H264LiveServerMediaSession *)clientData;
    subsess->checkForAuxSDPLine1();
}

void H264LiveServerMediaSession::checkForAuxSDPLine1()
{
    nextTask() = NULL;

    char const *dasl;
    if (fAuxSDPLine != NULL)
    {
        setDoneFlag();
    }
    else if (fDummyRTPSink != NULL && (dasl = fDummyRTPSink->auxSDPLine()) != NULL)
    {
        fAuxSDPLine = strDup(dasl);
        fDummyRTPSink = NULL;
        setDoneFlag();
    }
    else if (!fDoneFlag)
    {
        // try again after a brief delay:
        int uSecsToDelay = 100000; // 100 ms
        nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay,
                                                                 (TaskFunc *)checkForAuxSDPLine, this);
    }
}

char const *H264LiveServerMediaSession::getAuxSDPLine(RTPSink *rtpSink, FramedSource *inputSource)
{
    if (fAuxSDPLine != NULL)
    {
        return fAuxSDPLine;
    }

    if (fDummyRTPSink == NULL)
    {
        fDummyRTPSink = rtpSink;
        fDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);
        checkForAuxSDPLine(this);
    }
    envir().taskScheduler().doEventLoop(&fDoneFlag);

    return fAuxSDPLine;
}

FramedSource *H264LiveServerMediaSession::createNewStreamSource(unsigned clientSessionId, unsigned &estBitrate)
{
    estBitrate = 5000; // kbps, estimate

    H264VideoStreamSource *videoSource = H264VideoStreamSource::createNew(envir());
    if (videoSource == NULL)
    {
        return NULL;
    }

    return H264VideoStreamFramer::createNew(envir(), videoSource);
}

RTPSink *H264LiveServerMediaSession ::createNewRTPSink(Groupsock *rtpGroupsock,
                                                       unsigned char rtpPayloadTypeIfDynamic,
                                                       FramedSource *inputSource)
{
    // OutPacketBuffer::maxSize = 2000000;
    return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}

// H264VideoStreamSource 实现：
H264VideoStreamSource *H264VideoStreamSource::createNew(UsageEnvironment &env)
{
    return new H264VideoStreamSource(env);
}

H264VideoStreamSource::H264VideoStreamSource(UsageEnvironment &env) : FramedSource(env)
{
}

H264VideoStreamSource::~H264VideoStreamSource()
{
}

unsigned int H264VideoStreamSource::maxFrameSize() const
{
    return 100000; // 设置fMaxSize的值
}
FrameReader reader;
FILE *fp_bs = NULL;
int i = 0;
void H264VideoStreamSource::doGetNextFrame()
{
    //printf("--------------i = %d\n",i);
    if (!fp_bs)
    {
        printf("fb is nullptr\n");
        return;
    }
    int frame_length = 0;
    if (read_next_frame_length(&reader, &frame_length) > 0)
    {
        fFrameSize = frame_length;
        int rd_cnt = fread(fTo, 1, frame_length, fp_bs);
        if (rd_cnt != frame_length)
        {
            printf("fead error \n");
            return;
        }
    }
    else
    {
        return;
    }
    gettimeofday(&fPresentationTime, NULL);
    FramedSource::afterGetting(this);
    i++;
}

void H264VideoStreamSource::doStopGettingFrames()
{
    std::cout << "doStopGettingFrames" << std::endl;
}
int main(int argc, char **argv)
{
    printf("----------------main----------------\n");
    fp_bs = fopen("rtp_send.h264", "rb");
    if (!fp_bs)
    {
        printf("fopen error\n");
        return -1;
    }
    
    if (init_frame_reader("rtp_send.txt", &reader) == 0)
    {
        printf("Reading frame lengths from file:\n");
    }
    else
    {
        printf("Reading frame lengths error\n");
        return -1;
    }
    create_rtsp_server();
    close_frame_reader(&reader); // 关闭文件
    return 0;                    // only to prevent compiler warning
}

// static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms,
// 			   char const* streamName, char const* inputFileName) {
//   char* url = rtspServer->rtspURL(sms);
//   UsageEnvironment& env = rtspServer->envir();
//   env << "\n\"" << streamName << "\" stream, from the file \""
//       << inputFileName << "\"\n";
//   env << "Play this stream using the URL \"" << url << "\"\n";
//   delete[] url;
// }

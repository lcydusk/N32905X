/*
* 语音听写(iFly Auto Transform)技术能够实时地将语音转换成对应的文字。
*/
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "qisr.h"
#include "msp_cmn.h"
#include "msp_errors.h"
#include "speech_recognizer.h"



#define FRAME_LEN	640
#define	BUFFER_SIZE	4096
#define TTS_LEN		2048

//#define SRV_IP		"10.104.2.121"
#define SRV_IP		"210.14.68.48"
//#define SRV_PORT	8880
#define SRV_PORT	81
//#define SOCK_RQ		"GET http://10.104.2.121:8880/know/src/know.php?req="
//#define SOCK_RQ		"GET http://210.14.68.48:81/know/src/know.php?req="
#define SOCK_RQ		"GET http://210.14.68.48:81/getknow.php?req="

//#define CURL_KNOW_WEB	"curl -0 -s http://10.104.2.121:8880/know/src/know.php?req="
#define CURL_KNOW_WEB	"curl -0 -s http://210.14.68.48:81/know/src/know.php?req=\""
//#define CURL_REQ_TAIL	"&type=notnull\""
#define CURL_REQ_TAIL	"&pwd=abilix\n"
#define TTS_KNOW	"./tts_know "

#define PLAY_START  	"./aplay start.wav"
#define PLAY_LET_ME_SEE "./aplay let_me_see.wav"
#define PLAY_TO_GOOGLE	"./aplay let_me_google.wav"
#define PLAY_KNOWLEDGE  "./aplay tts_knowledge.wav"


static char *g_result = NULL;
static unsigned int g_buffersize = BUFFER_SIZE;
static char s_knowledge[TTS_LEN] = {0};
static char s_knldg_split[TTS_LEN] = {0};

volatile int flag_speak_done = 0;  //sj, it will be set 1 in on_speech_end() when speak done.
volatile int flag_result = 0;
volatile int flag_tts_done = 0;
volatile int sec_passed = 0; 


/* wav音频头部格式 */
typedef struct _wave_pcm_hdr
{
    char            riff[4];                // = "RIFF"
    int     size_8;                 // = FileSize - 8
    char            wave[4];                // = "WAVE"
    char            fmt[4];                 // = "fmt "
    int     fmt_size;       // = 下一个结构体的大小 : 16

    short int       format_tag;             // = PCM : 1
    short int       channels;               // = 通道数 : 1
    int     samples_per_sec;        // = 采样率 : 8000 | 6000 | 11025 | 16000
    int     avg_bytes_per_sec;      // = 每秒字节数 : samples_per_sec * bits_per_sample / 8
    short int       block_align;            // = 每采样点字节数 : wBitsPerSample / 8
    short int       bits_per_sample;        // = 量化比特数: 8 | 16

    char            data[4];                // = "data";
    int     data_size;              // = 纯数据长度 : FileSize - 44 
} wave_pcm_hdr;

/* 默认wav音频头部数据 */
wave_pcm_hdr default_wav_hdr = 
{
    { 'R', 'I', 'F', 'F' },
    0,
    {'W', 'A', 'V', 'E'},
    {'f', 'm', 't', ' '},
    16,
    1,
    1,
    16000,
    32000,
    2,
    16,
    {'d', 'a', 't', 'a'},
    0  
};

void timestamp()
{
    const struct tm *tm_ptr;
    time_t now = time ( 0 );
    tm_ptr = localtime ( &now );
    
    struct timeval tv;    
    gettimeofday(&tv,NULL);    

    printf("(%02d:%02d:%02d.%06ld) : ", tm_ptr->tm_hour, tm_ptr->tm_min, tm_ptr->tm_sec, tv.tv_usec);
}





//trim trailing
static void rtrim ( char *source_string, const char *trim_string )
{
    //printf("sjdb: src :(%s)\n", source_string);
    //printf("trim :(%s)\n", trim_string);
    int input_len = strlen(source_string);
    int trim_string_len = strlen(trim_string);
    char *input_value_ptr = source_string;
    const char *trim_string_ptr = trim_string;

    int escape_flag = 0;
    int i = 0, j = 0;

    //assert(source_string != NULL);
    //assert(trim_string != NULL);

    input_len = strlen(input_value_ptr);
    escape_flag = 0;
    /* delete the end character */
    for (i = input_len - 1; i >= 0; i--)
    {
        /* cycle the trim string, if the character isn't one of trim string,
         * escape the cycle, and move the string */
        for (j = 0; j < trim_string_len; j++)
        {
            if (input_value_ptr[i] == trim_string_ptr[j])
            {
                //printf("==");
                break;
            }
            if(j == (trim_string_len - 1))
            {
                escape_flag = 1;
            }
        }
        if(escape_flag)
        {
            break;
        }
    }
    input_value_ptr[i+1] = '\0'; //need i+2 under utf8 format
    //char * end_string = "&type=notnull"; //this is only used for out Web
    char * end_string = "&pwd=abilix"; //this is only used for out Web
    strcpy(&input_value_ptr[i+1], end_string);
    printf("sjdb: after trim (%s)\n", source_string);
}

//split the sentance and tts speak it.
static void split_tts(char* words)
{
	printf("sjdb: in split_tts().\n");
    char a_zh[4] = {0};
    char* punct = "。|？|！|；|：";   // ，
    char* p = words;
    char* s_n = p;
    int len = strlen(words);
	
	timestamp(); printf("sjdb: b4 while in split_tts().\n");
    while(*p) {
		printf("sjdb: in while of split_tts().\r");
        if(p-words >= len-1) break;
        strncat(a_zh, p, 3);
        if( (strstr(punct, a_zh) != NULL) || p-words > len-3) {
		    if(*s_n == 0 ) break;
			printf("sjdb: find a punctuation.\n");

            strncat(s_knldg_split, TTS_KNOW, strlen(TTS_KNOW));
            strncat(s_knldg_split, s_n, (p-words > len-3) ? (p-s_n+3) : (p-s_n) );
            timestamp();
            printf("sjdb: invoke %s\n", s_knldg_split);
            system(s_knldg_split);
            memset(s_knldg_split, 0, TTS_LEN);

            s_n = p+3; //the start of next substring
        }
        memset(a_zh, 0, 4);
        p++;
    }
	memset(words, 0, TTS_LEN);
}

static void tts_knowledge()
{
    flag_tts_done = 0;

    char s_send[BUFFER_SIZE*2] = {0};
    int recv_len = 0;
    //char s_cmd_tts[128+TTS_LEN] = {0};


    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));  //每个字节都用0填充

    serv_addr.sin_family = AF_INET;  //使用IPv4地址
    serv_addr.sin_addr.s_addr = inet_addr(SRV_IP);  //具体的IP地址
    serv_addr.sin_port = htons(SRV_PORT);  //端口

    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0) {
        printf("sjdb: connection failed.\n");
        return;
    }

    int i_send_len = strlen(SOCK_RQ) + strlen(g_result);
    //s_send = (char*)malloc(i_send_len);
    memset(s_send, 0, i_send_len);

    strncat(s_send, SOCK_RQ, strlen(SOCK_RQ));
    strncat(s_send, g_result, strlen(g_result));
    s_send[i_send_len] = 0x0a;
    timestamp();
    printf("sjdb: b4 send  %s\n", s_send);

    //send to srv

    send(sock, s_send, i_send_len+1, 0);
    timestamp();
    printf("sjdb: after sent.\n");

    memset(s_knowledge, 0, TTS_LEN);
    while(recv_len = recv(sock, s_knowledge, TTS_LEN, 0)) {
        if( strlen(s_knowledge) == 0)
        {
            printf("!!!No ans: (%s)\n", s_knowledge);
            printf("Recieve Data From Server %s Failed!\n", SRV_IP);
            break;
        }
        printf("ans: (%s)\n", s_knowledge);
        //split_tts(s_knowledge);
        //memset(s_knowledge, 0, TTS_LEN);

        if(recv_len > 60){
            popen(PLAY_TO_GOOGLE, "r");
        }

        timestamp(); printf("ans: (%s)\n", s_knowledge);
        split_tts(s_knowledge);
        //memset(s_knowledge, 0, TTS_LEN);
    }
    close(sock);
    if(recv_len == 0) {
        printf("!!!!Recieve Data From Server :%d\n", recv_len);
        //if( strlen(s_knowledge) != 0) {
        //    split_tts(s_knowledge);
        //}
    }

    flag_tts_done = 1;
}


static void show_result(char *string, char is_over)
{
    timestamp();
    printf("\nResult: [ %s ]", string);
    if(is_over) {
        rtrim(g_result, "。？ ");
        tts_knowledge();
    }
}



void on_result(const char *result, char is_last)
{
    if (result) {
        size_t left = g_buffersize - 1 - strlen(g_result);
        size_t size = strlen(result);
        if (left < size) {
            g_result = (char*)realloc(g_result, g_buffersize + BUFFER_SIZE);
            if (g_result)
                g_buffersize += BUFFER_SIZE;
            else {
                printf("mem alloc failed\n");
                return;
            }
        }
        strncat(g_result, result, size);

        show_result(g_result, is_last);
    }
}
void on_speech_begin()
{
    if (g_result)
    {
        free(g_result);
    }
    g_result = (char*)malloc(BUFFER_SIZE);
    g_buffersize = BUFFER_SIZE;
    memset(g_result, 0, g_buffersize);

    timestamp();
    printf("Start Listening...\n");
}
void on_speech_end(int reason)
{
    if (reason == END_REASON_VAD_DETECT) {
        timestamp();
        printf("\nSpeaking done \n");
    }
    else
        printf("\nRecognizer error %d\n", reason);
    flag_speak_done = 1; 		//sj
}


/* demo recognize the audio from microphone */
static void demo_mic(const char* session_begin_params)
{
    int errcode;
    sec_passed = 0 ;
    flag_speak_done = 0; //sj
    flag_result = 0; 

    struct speech_rec iat;

    struct speech_rec_notifier recnotifier = {
        on_result,
        on_speech_begin,
        on_speech_end
    };

    errcode = sr_init(&iat, session_begin_params, SR_MIC, &recnotifier);
    if (errcode) {
        printf("speech recognizer init failed\n");
        return;
    }
    errcode = sr_start_listening(&iat);
    if (errcode) {
        printf("start listen failed %d\n", errcode);
    }
    /* demo 15 seconds recording */
    while(sec_passed++ < 15) {
        sleep(1);
        //sj, break rec while speak_done detected. Usually, command words less than 5s.
        if(1 == flag_speak_done ) {
            printf("SJDB, %d sec passed, speak_done detected.\n", sec_passed);
            break;
        }
            
        if(sec_passed == 3 && flag_result == 0 ){
            popen(PLAY_LET_ME_SEE, "r");
        }

        if(14 == sec_passed) {
            printf("15 sec passed.\n");
        }
    }
    //wait for the tts speak finished.
    while(flag_tts_done != 1 && strlen(g_result) != 0 ) {
        printf("flag_tts_done :%d\r", flag_tts_done);
        usleep(200*1000);
        //sleep(1);
    }
    printf("stop listening\n");
    errcode = sr_stop_listening(&iat);
    if (errcode) {
        printf("stop listening failed %d\n", errcode);
    }
    else
        printf("sjdb: stop record from mic.\n");

    
    sr_uninit(&iat);
}


/* main thread: start/stop record ; query the result of recgonization.
 * record thread: record callback(data write)
 * helper thread: ui(keystroke detection)
 */
int main(int argc, char* argv[])
{
    int ret = MSP_SUCCESS;
    int upload_on =	1; /* whether upload the user word */
    /* login params, please do keep the appid correct */
    //const char* login_params = "appid = 58e4c070, work_dir = .";
    const char* login_params = "appid = 59546cd0, work_dir = .";
    int aud_src = 0; /* from mic or file */

    /*
    * See "iFlytek MSC Reference Manual"
    */
    const char* session_begin_params =
        "sub = iat, domain = iat, language = zh_cn, "
        "accent = mandarin, sample_rate = 16000 , vad_bos = 3000, vad_eos = 500, "
        "result_type = plain, result_encoding = utf8";

    /* Login first. the 1st arg is username, the 2nd arg is password
     * just set them as NULL. the 3rd arg is login paramertes
     * */
    ret = MSPLogin(NULL, NULL, login_params);
    if (MSP_SUCCESS != ret)	{
        printf("MSPLogin failed , Error code %d.\n",ret);
        goto exit; // login fail, exit the program
    }
    printf("Thanks for iflytek's voice SDK\n ");
again:
    timestamp();
    printf("Demo recognizing the speech from microphone\n");
    printf("Speak in 15 seconds\n");
    system(PLAY_START);
    timestamp();
    printf("B4 demo_mic.\n");
    demo_mic(session_begin_params);
    //printf("sjdb: out demo_mic.\n");
    sleep(2);
    goto again;

exit:
    printf("sjdb: before exit.\n");

    MSPLogout(); // Logout...

    return 0;
}

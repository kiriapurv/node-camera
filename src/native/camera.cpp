#include <node.h>
#include <v8.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/video/video.hpp>
#include <iostream>
#include <fstream>
#include <uv.h>
#include <vector>

#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif

using namespace v8;

//Define functions in scope
std::string stringValue(Local<Value> value);

int m_brk;
int32_t preview_width, preview_height;
uv_async_t async;
uv_loop_t *loop;

struct TMessage {
    Persistent<Function> callBack;
    cv::VideoCapture *capture;
    bool resize;
    int32_t width, height;
    bool window;
    std::string codec;
    ~TMessage() {
        callBack.Reset();
        delete capture;
    }
};

struct AsyncMessage {
    std::vector<unsigned char> image;
    cv::Mat frame;
    bool window;
};

TMessage *message;

void updateAsync(uv_async_t* req, int status) {
    
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    
    AsyncMessage* asyncMessage = (AsyncMessage*) req->data;
    
    Local<Function> callBack = Local<Function>::New(isolate,message->callBack);
    
    if(asyncMessage->window && asyncMessage->frame.size().height > 0 && asyncMessage->frame.size().width > 0 ) {
        cv::imshow("Preview", asyncMessage->frame);
        cv::waitKey(20);
    }
    
    Local<Array> arr = Array::New(isolate,asyncMessage->image.size());
    int pos = 0;
    for(unsigned char c : asyncMessage->image) {
        arr->Set(pos++,Integer::New(isolate,c));
    }
    
    Local<Value> argv[] = {
            arr
    };
    
    callBack->Call(isolate->GetCurrentContext()->Global(), 1, argv);
    asyncMessage->image.clear();
    asyncMessage->frame.release();
    
    delete asyncMessage;
}

void CameraOpen(uv_work_t* req) {
    TMessage* message = (TMessage*) req->data;
    
    cv::Mat tmp, rsz;
    
    while(m_brk > 0 && message->capture->isOpened()) {  
        
        AsyncMessage *msg = new AsyncMessage();
        msg->image = std::vector<unsigned char>();
        msg->window = message->window;
        
        //Capture Frame From WebCam
        message->capture->read(tmp);
        
        if(message->resize) {
            cv::Size size = cv::Size(message->width,message->height);
            cv::resize(tmp,rsz,size);
            msg->frame = rsz;
            //Update Size
            preview_width = message->width;
            preview_height = message->height;
        } else {
            msg->frame = tmp;
            //Update Size
            preview_width = tmp.size().width;
            preview_height = tmp.size().height;
        }

        //TODO : Add image parameters here

        std::vector<int> compression_parameters = std::vector<int>(2);
        compression_parameters[0] = CV_IMWRITE_JPEG_QUALITY;
        compression_parameters[1] = 85;
        
        //Encode to jpg
        if(message->resize) {
            cv::imencode(message->codec,rsz,msg->image,compression_parameters);   
        } else {
            cv::imencode(message->codec,tmp,msg->image,compression_parameters);
        }
        
        compression_parameters.clear();
        
        if(message->window && tmp.size().height > 0 && tmp.size().width > 0 ) {
            cv::imshow("Preview", msg->frame);
            cv::waitKey(20);
        }
        
        async.data  = msg;
        uv_async_send(&async);
        
    }
    rsz.release();
    tmp.release();
    
}

void CameraClose(uv_work_t* req, int status) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    TMessage* message = (TMessage*) req->data;
    message->capture->release();
    delete message->capture;
    delete req;
}

void IsOpen(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    args.GetReturnValue().Set(Boolean::New(isolate,(m_brk == 1) ? TRUE : FALSE));
}

void GetPreviewSize(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    Local<Object> obj = Object::New(isolate);
    obj->Set(String::NewFromUtf8(isolate,"width"), Integer::New(isolate,preview_width));
    obj->Set(String::NewFromUtf8(isolate,"height"), Integer::New(isolate,preview_height));
    args.GetReturnValue().Set(obj);
}

void Open(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    
    loop = uv_default_loop();
    message = new TMessage;
    
    m_brk = 1;
    if(!args[0]->IsFunction()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate,"First argument must be frame callback function")));
        return;
    }
    
    //Default Arguments
    message->codec = std::string(".jpg");
    Local<Value> input = Number::New(isolate,0);
    std::string inputString;
    
    //Check if size is passed
    if(args.Length() == 2) {
        //Second parameter is parameters, which contains on Json object having width and height
        if(!args[1]->IsObject()) {
            isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate,"Second argument must be object")));
            return;
        }
        Local<Object> params = args[1]->ToObject();
        if(params->Has(String::NewFromUtf8(isolate,"width"))) {
            message->width = params->Get(String::NewFromUtf8(isolate,"width"))->Int32Value();
            message->height = params->Get(String::NewFromUtf8(isolate,"height"))->Int32Value();
        }
        if(params->Has(String::NewFromUtf8(isolate,"window"))) {
            message->window = params->Get(String::NewFromUtf8(isolate,"window"))->BooleanValue();
        }
        if(params->Has(String::NewFromUtf8(isolate,"codec"))) {
            Local<String> val = params->Get(String::NewFromUtf8(isolate,"codec"))->ToString();
            message->codec = stringValue(val);
        }
        if(params->Has(String::NewFromUtf8(isolate,"input"))) {
            Local<Value> input = params->Get(String::NewFromUtf8(isolate,"input"));
            if(!input->IsNumber()) {
                inputString = stringValue(input);
            }
        }
        
    }
    if(message->window) {
        cv::namedWindow("Preview",1);   
    }
    
    message->callBack.Reset(isolate,Handle<Function>::Cast(args[0]));
    
    //Initiate OpenCV WebCam
    message->capture = new cv::VideoCapture();
    if(input->IsNumber()) {
        message->capture->open((int)input->Int32Value());
    } else if(!inputString.empty()) {
        message->capture->open(inputString);
    }
    cv::waitKey(10);
    
    uv_work_t* req = new uv_work_t();
    req->data = message;
    
    async = uv_async_t();
    
    uv_async_init(loop,&async,(uv_async_cb)updateAsync);
    uv_queue_work(loop, req, CameraOpen,(uv_after_work_cb) CameraClose);
    
    args.GetReturnValue().Set(String::NewFromUtf8(isolate,"ok"));
}

void Close(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    
    m_brk = 0;
    uv_loop_close(loop);
    uv_close((uv_handle_t *) &async, NULL);
    cv::destroyWindow("Preview");
    args.GetReturnValue().Set(String::NewFromUtf8(isolate,"ok"));
}

void init(Handle<Object> exports) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    exports->Set(String::NewFromUtf8(isolate,"Open"), FunctionTemplate::New(isolate,Open)->GetFunction());
    exports->Set(String::NewFromUtf8(isolate,"Close"), FunctionTemplate::New(isolate,Close)->GetFunction());
    exports->Set(String::NewFromUtf8(isolate,"IsOpen"), FunctionTemplate::New(isolate,IsOpen)->GetFunction());
    exports->Set(String::NewFromUtf8(isolate,"GetPreviewSize"), FunctionTemplate::New(isolate,GetPreviewSize)->GetFunction());
}

std::string stringValue(Local<Value> value) {
    if(value->IsString()){
        //Alloc #1
        char * buffer = (char*) malloc(sizeof(char) * value->ToString()->Utf8Length());
        value->ToString()->WriteUtf8(buffer,value->ToString()->Utf8Length());
        std::string ret(buffer);
        free(buffer);
        return ret;
    } else {
        return "";
    }
}

NODE_MODULE(camera, init);
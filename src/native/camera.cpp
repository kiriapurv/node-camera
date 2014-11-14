#include <node.h>
#include <v8.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/video/video.hpp>
#include <iostream>
#include <fstream>
#include <vector>

#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif

using namespace v8;

//Define functions in scope
std::string* stringValue(Local<Value> value);

int m_brk;
int32_t preview_width, preview_height;
uv_async_t *async;
uv_loop_t *loop;

struct TMessage {
    Persistent<Function> callBack;
    cv::VideoCapture *capture;
    bool resize;
    int32_t width, height;
    bool window;
    std::string codec;
    ~TMessage() {
        callBack.Clear();
        callBack.Dispose();
        delete capture;
    }
};

struct AsyncMessage {
    std::vector<unsigned char> *image;
    cv::Mat *frame;
    bool window;
    Persistent<Function> callBack;
};

void updateAsync(uv_async_t* req, int status) {
    
    HandleScope scope;
    
    AsyncMessage* message = (AsyncMessage*) req->data;
    
    //Update Size
    preview_width = message->frame->size().width;
    preview_height = message->frame->size().height;
    
    Persistent<Function> callBack = message->callBack;
    
    if(message->window) {
        cv::imshow("Preview", *message->frame);
        cv::waitKey(20);
    }
    
    Local<Array> arr = Array::New(message->image->size());
    int pos = 0;
    for(unsigned char c : *message->image) {
        arr->Set(pos++,Integer::New(c));
    }
    
    Local<Value> argv[] = {
            arr
    };
    
    callBack->Call(Context::GetCurrent()->Global(), 1, argv);
    
    delete message->frame;
    delete message->image;
}

void CameraOpen(uv_work_t* req) {
    //HandleScope scope;
    TMessage* message = (TMessage*) req->data;
    Persistent<Function> callBack = message->callBack;
    
    while(m_brk > 0 && message->capture->isOpened()) {  
        cv::Mat *tmp = new cv::Mat;
        cv::Mat *rsz;
        
        AsyncMessage* msg = new AsyncMessage;
        msg->image = new std::vector<uchar>();
        msg->window = message->window;
        
        //Capture Frame From WebCam
        message->capture->read(*tmp);
        
        if(message->resize) {
            rsz = new cv::Mat;
            cv::Size *size = new cv::Size(message->width,message->height);
            cv::resize(*tmp,*rsz,*size);
            msg->frame = rsz;
            delete size;
        } else {
            msg->frame = tmp;
        }

        //TODO : Add image parameters here

        //Encode to jpg
        if(message->resize) {
            cv::imencode(message->codec,*rsz,*msg->image);   
        } else {
            cv::imencode(message->codec,*tmp,*msg->image);
        }
        
        msg->callBack = callBack;
        async->data  = msg;
        uv_async_send(async);
        
        if(!message->resize) {
            delete tmp;
        }
    }
    
}

void CameraClose(uv_work_t* req, int status) {
    HandleScope scope;
    TMessage* message = (TMessage*) req->data;
    message->capture->release();
    delete message->capture;
    delete req;
}

Handle<Value> IsOpen(const Arguments& args) {
    HandleScope scope;
    return scope.Close(Boolean::New((m_brk == 1) ? TRUE : FALSE));
}

Handle<Value> GetPreviewSize(const Arguments& args) {
    HandleScope scope;
    Local<Object> obj = Object::New();
    obj->Set(String::NewSymbol("width"), Integer::New(preview_width));
    obj->Set(String::NewSymbol("height"), Integer::New(preview_height));
    return scope.Close(obj);
}

Handle<Value> Open(const Arguments& args) {
    
    HandleScope scope;
    loop = uv_default_loop();
    TMessage* message = new TMessage;
    
    m_brk = 1;
    if(!args[0]->IsFunction()) {
        return ThrowException(Exception::TypeError(String::New("First argument must be frame callback function")));
    }
    
    //Default Arguments
    message->codec = std::string(".jpg");
    Local<Value> input = Number::New(0);
    std::string* inputString;
    
    //Check if size is passed
    if(args.Length() == 2) {
        //Second parameter is parameters, which contains on Json object having width and height
        if(!args[1]->IsObject()) {
            return ThrowException(Exception::TypeError(String::New("Second argument must be object")));
        }
        Local<Object> params = args[1]->ToObject();
        if(params->Has(String::NewSymbol("width"))) {
            message->width = params->Get(String::NewSymbol("width"))->Int32Value();
            message->height = params->Get(String::NewSymbol("height"))->Int32Value();
        }
        if(params->Has(String::NewSymbol("window"))) {
            message->window = params->Get(String::NewSymbol("window"))->BooleanValue();
        }
        if(params->Has(String::NewSymbol("codec"))) {
            Local<String> val = params->Get(String::NewSymbol("codec"))->ToString();
            message->codec = *stringValue(val);
        }
        if(params->Has(String::NewSymbol("input"))) {
            input = params->Get(String::NewSymbol("input"));
            if(!input->IsNumber()) {
                inputString = stringValue(input);
            }
        }
        
    }
    if(message->window) {
        cv::namedWindow("Preview",1);   
    }
    
    Local<Function> localFunc = Local<Function>::Cast(args[0]);
    
    message->callBack = Persistent<Function>::New(localFunc);
    //Initiate OpenCV WebCam
    message->capture = new cv::VideoCapture();
    if(input->IsNumber()) {
        message->capture->open((int)input->Int32Value());
    } else {
        message->capture->open(*inputString);
    }
    cv::waitKey(10);
    
    uv_work_t* req = new uv_work_t();
    req->data = message;
    
    async = new uv_async_t();
    
    uv_async_init(loop,async,updateAsync);
    uv_queue_work(loop, req, CameraOpen,(uv_after_work_cb) CameraClose);
    
    //Free resources
    //free #1
    if(!input->IsNumber()) {
        delete inputString;
    }
    
    return scope.Close(String::New("ok"));
}

Handle<Value> Close(const Arguments& args) {
    
    HandleScope scope;
    
    m_brk = 0;
    uv_run(loop,UV_RUN_DEFAULT);
    uv_close((uv_handle_t *) async, NULL);
    delete async;
    delete loop;
    cv::destroyWindow("Preview");
    return scope.Close(String::New("ok"));
}

void init(Handle<Object> exports) {
    exports->Set(String::NewSymbol("Open"), FunctionTemplate::New(Open)->GetFunction());
    exports->Set(String::NewSymbol("Close"), FunctionTemplate::New(Close)->GetFunction());
    exports->Set(String::NewSymbol("IsOpen"), FunctionTemplate::New(IsOpen)->GetFunction());
    exports->Set(String::NewSymbol("GetPreviewSize"), FunctionTemplate::New(GetPreviewSize)->GetFunction());
}

std::string* stringValue(Local<Value> value) {
    if(value->IsString()){
        //Alloc #1
        char * buffer = (char*) malloc(sizeof(char) * value->ToString()->Length());
        value->ToString()->WriteAscii(buffer,0,value->ToString()->Length());
        std::string *ret = new std::string(buffer);
        free(buffer);
        return ret;
    } else {
        return new std::string("");
    }
}

NODE_MODULE(camera, init);
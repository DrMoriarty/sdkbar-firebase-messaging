#include "CloudMessaging.hpp"
#include "Firebase.hpp"
#include "scripting/js-bindings/manual/cocos2d_specifics.hpp"
#include "scripting/js-bindings/manual/js_manual_conversions.h"
#include <sstream>
#include "base/CCDirector.h"
#include "base/CCScheduler.h"
#include "utils/PluginUtils.h"
#include "firebase/messaging.h"

static void printLog(const char* str) {
    CCLOG("%s", str);
}

static std::string appToken;

static CallbackFrame *messagingCallback = NULL;

///////////////////////////////////////
//
//  MyListener
//
///////////////////////////////////////

class MyListener: public firebase::messaging::Listener {
public:
    virtual ~MyListener() {
    }

    virtual void OnMessage(const firebase::messaging::Message& message) {
        printLog("Cloud messaging message:");
        printLog(message.raw_data.c_str());
        for(std::map<std::string, std::string>::const_iterator it = message.data.begin(); it != message.data.end(); it++) {
            std::string key = it->first;
            std::string value = it->second;
            printLog(key.c_str());
            printLog(value.c_str());
        }
        if(message.notification != NULL) {
            printLog("Cloud message notification:");
            printLog(message.notification->title.c_str());
            printLog(message.notification->body.c_str());
            printLog(message.notification->icon.c_str());
            printLog(message.notification->sound.c_str());
            printLog(message.notification->badge.c_str());
            printLog(message.notification->tag.c_str());
            printLog(message.notification->color.c_str());
            printLog(message.notification->click_action.c_str());
        }
        if(messagingCallback != NULL) {
            // send message json
            std::map<std::string, std::string> notification;
            notification["title"] = message.notification->title;
            notification["body"] = message.notification->body;
            notification["icon"] = message.notification->icon;
            notification["sound"] = message.notification->sound;
            notification["badge"] = message.notification->badge;
            notification["tag"] = message.notification->tag;
            notification["color"] = message.notification->color;
            notification["click_action"] = message.notification->click_action;

            cocos2d::Director::getInstance()->getScheduler()->
                performFunctionInCocosThread([notification] {

                                                 JSAutoRequest rq(messagingCallback->cx);
                                                 JSAutoCompartment ac(messagingCallback->cx, messagingCallback->_ctxObject.ref());

                                                 JS::RootedObject proto(messagingCallback->cx);
                                                 JS::RootedObject parent(messagingCallback->cx);
                                                 JS::RootedObject jsRet(messagingCallback->cx, JS_NewObject(messagingCallback->cx, NULL, proto, parent));

                                                 for(std::map<std::string, std::string>::const_iterator it = notification.begin(); it != notification.end(); it++) {
                                                     std::string key = it->first;
                                                     std::string value = it->second;
                                                     JS::RootedValue val(messagingCallback->cx); 
                                                     val = std_string_to_jsval(messagingCallback->cx, value);
                                                     JS_SetProperty(messagingCallback->cx, jsRet, key.c_str(), val);
                                                 }

                                                 jsval arg = OBJECT_TO_JSVAL(jsRet);

                                                 JS::AutoValueVector valArr(messagingCallback->cx);
                                                 valArr.append(arg);
                                                 JS::HandleValueArray funcArgs = JS::HandleValueArray::fromMarkedLocation(1, valArr.begin());
                                                 messagingCallback->call(funcArgs);
                                             });
        }
    }

    virtual void OnTokenReceived(const char* token) {
        printLog("Cloud messaging token:");
        printLog(token);
        appToken = token;
    }
};

static MyListener* myListener = NULL;

///////////////////////////////////////
//
//  Plugin Init
//
///////////////////////////////////////

static bool jsb_cloudmessaging_init(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_cloudmessaging_init");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 0) {

        printLog("[FirebaseCloudMessaging] Init plugin");
        firebase::App* app = firebase_app();
        if(app == NULL) {
            printLog("Firebase not initialized!");
            rec.rval().set(JSVAL_FALSE);
            return false;
        }
        myListener = new MyListener();
        if(firebase::messaging::Initialize(*app, myListener) == firebase::kInitResultSuccess) {
            rec.rval().set(JSVAL_TRUE);
            return true;
        } else {
            rec.rval().set(JSVAL_FALSE);
            return false;
        }
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

///////////////////////////////////////
//
//  JS Api
//
///////////////////////////////////////

static bool jsb_cloudmessaging_get_token(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_cloudmessaging_get_token");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 0) {
        rec.rval().set(std_string_to_jsval(cx, appToken));
        return true;
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}

static bool jsb_cloudmessaging_set_callback(JSContext *cx, uint32_t argc, jsval *vp)
{
    printLog("jsb_cloudmessging_set_callback");
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull());
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    if(argc == 2) {
        messagingCallback = new CallbackFrame(cx, obj, args.get(1), args.get(0));
        rec.rval().set(JSVAL_TRUE);
        return true;
    } else {
        JS_ReportError(cx, "Invalid number of arguments");
        return false;
    }
}


///////////////////////////////////////
//
//  Register JS API
//
///////////////////////////////////////

void register_all_cloudmessaging_framework(JSContext* cx, JS::HandleObject obj) {
    printLog("[FirebaseCloudMessaging] register js interface");
    JS::RootedObject ns(cx);
    get_or_create_js_obj(cx, obj, "messaging", &ns);

    JS_DefineFunction(cx, ns, "init", jsb_cloudmessaging_init, 0, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineFunction(cx, ns, "get_token", jsb_cloudmessaging_get_token, 0, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineFunction(cx, ns, "set_callback", jsb_cloudmessaging_set_callback, 2, JSPROP_ENUMERATE | JSPROP_PERMANENT);

    //JS_DefineFunction(cx, ns, "get_boolean", jsb_admob_get_boolean, 1, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    //JS_DefineFunction(cx, ns, "get_integer", jsb_admob_get_integer, 1, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    //JS_DefineFunction(cx, ns, "get_double", jsb_admob_get_double, 1, JSPROP_ENUMERATE | JSPROP_PERMANENT);


}

/*==============================================================================

    ofxVisualProgramming: A visual programming patching environment for OF

    Copyright (c) 2018 Emanuele Mazza aka n3m3da <emanuelemazza@d3cod3.org>

    ofxVisualProgramming is distributed under the MIT License.
    This gives everyone the freedoms to use ofxVisualProgramming in any context:
    commercial or non-commercial, public or private, open or closed source.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
    OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    See https://github.com/d3cod3/ofxVisualProgramming for documentation

==============================================================================*/

#ifndef OFXVP_BUILD_WITH_MINIMAL_OBJECTS

#include "KinectGrabber.h"

//--------------------------------------------------------------
KinectGrabber::KinectGrabber() : PatchObject("kinect grabber"){

    this->numInlets  = 0;
    this->numOutlets = 2;

    _outletParams[0] = new ofTexture(); // video (IR or RGB)
    _outletParams[1] = new ofTexture(); // depth
    _outletParams[2] = new ofxKinect(); // kinect reference

    this->initInletsState();

    isNewObject         = false;

    posX = posY = drawW = drawH = 0.0f;

    kinectWidth         = 640;
    kinectHeight        = 480;

    deviceID            = 0;
    isIR                = false;

    nearThreshold       = 230.0f;
    farThreshold        = 70.0f;

    needReset           = false;

    weHaveKinect        = false;

    loaded                  = false;

    this->setIsResizable(true);
    this->setIsTextureObj(true);

    prevW                   = this->width;
    prevH                   = this->height;
}

//--------------------------------------------------------------
void KinectGrabber::newObject(){
    PatchObject::setName( this->objectName );

    this->addOutlet(VP_LINK_TEXTURE,"kinectImage");
    this->addOutlet(VP_LINK_TEXTURE,"kinectDepth");

    this->setCustomVar(static_cast<float>(deviceID),"DEVICE_ID");
    this->setCustomVar(static_cast<float>(isIR),"INFRARED");
    this->setCustomVar(nearThreshold,"NEAR_THRESH");
    this->setCustomVar(farThreshold,"FAR_THRESH");

    this->setCustomVar(prevW,"OBJ_WIDTH");
    this->setCustomVar(prevH,"OBJ_HEIGHT");
}

//--------------------------------------------------------------
void KinectGrabber::setupObjectContent(shared_ptr<ofAppGLFWWindow> &mainWindow){

    int numKinects = ofxKinect::numAvailableDevices();
    if(numKinects > 0){
        ofLog(OF_LOG_NOTICE,"KINECT devices available: %i",numKinects);
        for(int i=0;i<numKinects;i++){
            devicesVector.push_back("Kinect Device "+ofToString(i));
            devicesID.push_back(i);
        }
        weHaveKinect = true;
    }
    
}

//--------------------------------------------------------------
void KinectGrabber::updateObjectContent(map<int,shared_ptr<PatchObject>> &patchObjects){

    if(needReset){
        needReset = false;
        if(weHaveKinect){
            resetKinectSettings(deviceID);
        }
    }

    // KINECT UPDATE
    if(weHaveKinect && static_cast<ofxKinect *>(_outletParams[2])->isInitialized() && static_cast<ofxKinect *>(_outletParams[2])->isConnected()){
        static_cast<ofxKinect *>(_outletParams[2])->update();
        if(static_cast<ofxKinect *>(_outletParams[2])->isFrameNew()){
            *static_cast<ofTexture *>(_outletParams[0]) = static_cast<ofxKinect *>(_outletParams[2])->getTexture();

            cleanImage.setFromPixels(static_cast<ofxKinect *>(_outletParams[2])->getDepthPixels());
            cleanImage.updateTexture();

            grayThreshNear = cleanImage;
            grayThreshFar = cleanImage;
            grayThreshNear.threshold(nearThreshold, true);
            grayThreshFar.threshold(farThreshold);

            grayThreshNear.updateTexture();
            grayThreshFar.updateTexture();

            cvAnd(grayThreshNear.getCvImage(), grayThreshFar.getCvImage(), cleanImage.getCvImage(), nullptr);
            cleanImage.flagImageChanged();
            cleanImage.updateTexture();

            colorCleanImage = cleanImage;
            colorCleanImage.updateTexture();

            *static_cast<ofTexture *>(_outletParams[1]) = colorCleanImage.getTexture();
        }
    }

    if(!loaded){
        loaded = true;
        if(weHaveKinect){

            colorCleanImage.allocate(kinectWidth, kinectHeight);
            cleanImage.allocate(kinectWidth, kinectHeight);
            grayThreshNear.allocate(kinectWidth, kinectHeight);
            grayThreshFar.allocate(kinectWidth, kinectHeight);

            loadKinectSettings();

            static_cast<ofxKinect *>(_outletParams[2])->setRegistration(true);
            static_cast<ofxKinect *>(_outletParams[2])->init(isIR,true,true);
            static_cast<ofxKinect *>(_outletParams[2])->open(deviceID);
            static_cast<ofxKinect *>(_outletParams[2])->setCameraTiltAngle(0);

        }
    }

}

//--------------------------------------------------------------
void KinectGrabber::drawObjectContent(ofxFontStash *font, shared_ptr<ofBaseGLRenderer>& glRenderer){
    // background
    if(scaledObjW*canvasZoom > 90.0f){
        ofSetColor(34,34,34);
        if(this->numInlets>0){
            ofDrawRectangle(objOriginX - (IMGUI_EX_NODE_PINS_WIDTH_NORMAL*this->scaleFactor/canvasZoom), objOriginY-(IMGUI_EX_NODE_HEADER_HEIGHT*this->scaleFactor/canvasZoom),scaledObjW + (IMGUI_EX_NODE_PINS_WIDTH_NORMAL*this->scaleFactor/canvasZoom),scaledObjH + (((IMGUI_EX_NODE_HEADER_HEIGHT+IMGUI_EX_NODE_FOOTER_HEIGHT)*this->scaleFactor)/canvasZoom) );
        }else{
            ofDrawRectangle(objOriginX, objOriginY-(IMGUI_EX_NODE_HEADER_HEIGHT*this->scaleFactor/canvasZoom),scaledObjW,scaledObjH + (((IMGUI_EX_NODE_HEADER_HEIGHT+IMGUI_EX_NODE_FOOTER_HEIGHT)*this->scaleFactor)/canvasZoom) );
        }
    }

    // kinect image
    if(weHaveKinect && static_cast<ofxKinect *>(_outletParams[2])->isInitialized() && static_cast<ofxKinect *>(_outletParams[2])->isConnected() && static_cast<ofTexture *>(_outletParams[0])->isAllocated()){
        ofSetColor(255);
        drawNodeOFTexture(*static_cast<ofTexture *>(_outletParams[0]), posX, posY, drawW, drawH, objOriginX, objOriginY, scaledObjW, scaledObjH, canvasZoom, this->scaleFactor);
    }
}

//--------------------------------------------------------------
void KinectGrabber::drawObjectNodeGui( ImGuiEx::NodeCanvas& _nodeCanvas ){

    // CONFIG GUI inside Menu
    if(_nodeCanvas.BeginNodeMenu()){
        ImGui::Separator();
        ImGui::Separator();
        ImGui::Separator();

        if (ImGui::BeginMenu("CONFIG"))
        {

            drawObjectNodeConfig();

            ImGui::EndMenu();
        }
        _nodeCanvas.EndNodeMenu();
    }

    // Visualize (Object main view)
    if( _nodeCanvas.BeginNodeContent(ImGuiExNodeView_Visualise) ){

        // get imgui node translated/scaled position/dimension for drawing textures in OF
        objOriginX = (ImGui::GetWindowPos().x + ((IMGUI_EX_NODE_PINS_WIDTH_NORMAL - 1)*this->scaleFactor) - _nodeCanvas.GetCanvasTranslation().x)/_nodeCanvas.GetCanvasScale();
        objOriginY = (ImGui::GetWindowPos().y - _nodeCanvas.GetCanvasTranslation().y)/_nodeCanvas.GetCanvasScale();
        scaledObjW = this->width - (IMGUI_EX_NODE_PINS_WIDTH_NORMAL*this->scaleFactor/_nodeCanvas.GetCanvasScale());
        scaledObjH = this->height - ((IMGUI_EX_NODE_HEADER_HEIGHT+IMGUI_EX_NODE_FOOTER_HEIGHT)*this->scaleFactor/_nodeCanvas.GetCanvasScale());

        // save object dimensions (for resizable ones)
        if(this->width != prevW){
            prevW = this->width;
            this->setCustomVar(prevW,"OBJ_WIDTH");
        }
        if(this->width != prevH){
            prevH = this->height;
            this->setCustomVar(prevH,"OBJ_HEIGHT");
        }

        _nodeCanvas.EndNodeContent();
    }

    // get imgui canvas zoom
    canvasZoom = _nodeCanvas.GetCanvasScale();

}

//--------------------------------------------------------------
void KinectGrabber::drawObjectNodeConfig(){
    if(weHaveKinect){
        ImGui::Spacing();
        ImGui::Text("Kinect motion sensing input device (Xbox 360 model)");
        ImGui::Text("Format: %ix%i",kinectWidth, kinectHeight);
        ImGui::Spacing();
        if(ImGui::BeginCombo("Device", devicesVector.at(deviceID).c_str() )){
            for(int i=0; i < devicesVector.size(); ++i){
                bool is_selected = (deviceID == i );
                if (ImGui::Selectable(devicesVector.at(i).c_str(), is_selected)){
                    resetKinectSettings(i);
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        ImGui::Spacing();
        if(ImGui::Checkbox("Infrared",&isIR)){
            this->setCustomVar(static_cast<float>(isIR),"INFRARED");
            resetKinectImage(isIR);
        }
        ImGui::Spacing();
        if(ImGui::SliderFloat("Near Threshold",&nearThreshold,0,255)){
            this->setCustomVar(nearThreshold,"NEAR_THRESH");
        }
        ImGui::Spacing();
        if(ImGui::SliderFloat("Far Threshold",&farThreshold,0,255)){
            this->setCustomVar(farThreshold,"FAR_THRESH");
        }
    }else{
        ImGui::Spacing();
        ImGui::Text("No Kinect sensor found!");
    }


    ImGuiEx::ObjectInfo(
                "Opens a compatible Kinect sensor device",
                "https://mosaic.d3cod3.org/reference.php?r=kinect-grabber", scaleFactor);
}

//--------------------------------------------------------------
void KinectGrabber::removeObjectContent(bool removeFileFromData){
    static_cast<ofxKinect *>(_outletParams[2])->setCameraTiltAngle(0);
    static_cast<ofxKinect *>(_outletParams[2])->close();
}

//--------------------------------------------------------------
void KinectGrabber::loadKinectSettings(){
    if(static_cast<int>(floor(this->getCustomVar("DEVICE_ID"))) >= 0){
        deviceID = static_cast<int>(floor(this->getCustomVar("DEVICE_ID")));
    }else{
        deviceID = 0;
        this->setCustomVar(static_cast<float>(deviceID),"DEVICE_ID");
    }

    if(static_cast<int>(floor(this->getCustomVar("INFRARED"))) >= 0){
        isIR = static_cast<bool>(floor(this->getCustomVar("INFRARED")));
    }else{
        isIR = false;
        this->setCustomVar(static_cast<float>(isIR),"INFRARED");
    }

    nearThreshold = this->getCustomVar("NEAR_THRESH");
    farThreshold = this->getCustomVar("FAR_THRESH");

}

//--------------------------------------------------------------
void KinectGrabber::resetKinectSettings(int devID){

    if(devID!=deviceID){

        ofLog(OF_LOG_NOTICE,"Changing Device to: %s",devicesVector[devID].c_str());

        deviceID = devID;
        this->setCustomVar(static_cast<float>(deviceID),"DEVICE_ID");


        if(static_cast<ofxKinect *>(_outletParams[2])->isInitialized()){
            static_cast<ofxKinect *>(_outletParams[2])->setCameraTiltAngle(0);
            static_cast<ofxKinect *>(_outletParams[2])->close();

            _outletParams[2] = new ofxKinect();
            static_cast<ofxKinect *>(_outletParams[2])->setRegistration(true);
            static_cast<ofxKinect *>(_outletParams[2])->init(isIR,true,true);
            static_cast<ofxKinect *>(_outletParams[2])->open(deviceID);
        }
    }
}

//--------------------------------------------------------------
void KinectGrabber::resetKinectImage(bool ir){

    if(ir!=isIR){

        isIR = ir;

        this->setCustomVar(static_cast<float>(ir),"INFRARED");

        if(static_cast<ofxKinect *>(_outletParams[2])->isInitialized()){
            static_cast<ofxKinect *>(_outletParams[2])->setCameraTiltAngle(0);
            static_cast<ofxKinect *>(_outletParams[2])->close();

            _outletParams[2] = new ofxKinect();
            static_cast<ofxKinect *>(_outletParams[2])->setRegistration(true);
            static_cast<ofxKinect *>(_outletParams[2])->init(ir,true,true);
            static_cast<ofxKinect *>(_outletParams[2])->open(deviceID);
        }
    }

}


OBJECT_REGISTER( KinectGrabber, "kinect grabber", OFXVP_OBJECT_CAT_TEXTURE)

#endif

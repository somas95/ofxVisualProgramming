/*==============================================================================

    ofxVisualProgramming: A visual programming patching environment for OF

    Copyright (c) 2019 Emanuele Mazza aka n3m3da <emanuelemazza@d3cod3.org>

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

// Unavailable on windows.
#if defined(TARGET_LINUX) || defined(TARGET_OSX)

#include "VideoSender.h"

//--------------------------------------------------------------
VideoSender::VideoSender() : PatchObject(){

    this->numInlets  = 2;
    this->numOutlets = 0;

    _inletParams[0] = new ofTexture(); // input
    _inletParams[1] = new float();      // bang
    *(float *)&_inletParams[1] = 0.0f;

    this->initInletsState();

    isGUIObject         = true;
    this->isOverGUI     = true;

    isNewObject         = false;

    posX = posY = drawW = drawH = 0.0f;

    needToGrab          = false;
}

//--------------------------------------------------------------
void VideoSender::newObject(){
    this->setName(this->objectName);
    this->addInlet(VP_LINK_TEXTURE,"input");
}

//--------------------------------------------------------------
void VideoSender::setupObjectContent(shared_ptr<ofAppGLFWWindow> &mainWindow){

    gui = new ofxDatGui( ofxDatGuiAnchor::TOP_RIGHT );
    gui->setAutoDraw(false);
    gui->setUseCustomMouse(true);
    gui->setWidth(this->width);

    header = gui->addHeader("CONFIG",false);
    header->setUseCustomMouse(true);
    header->setCollapsable(true);

    recButton = gui->addToggle("SEND");
    recButton->setUseCustomMouse(true);
    recButton->setLabelAlignment(ofxDatGuiAlignment::CENTER);

    gui->onToggleEvent(this, &VideoSender::onToggleEvent);

    gui->setPosition(0,this->height - header->getHeight());
    gui->collapse();
    header->setIsCollapsed(true);

    captureFbo.allocate( STANDARD_TEXTURE_WIDTH, STANDARD_TEXTURE_HEIGHT, GL_RGB );
    ndiSender.setMetaData("Mosaic NDI sender", "video sender", "ofxNDI", "0.0.0", "", "", "");


}

//--------------------------------------------------------------
void VideoSender::updateObjectContent(map<int,shared_ptr<PatchObject>> &patchObjects, ofxThreadedFileDialog &fd){

    gui->update();
    header->update();
    if(!header->getIsCollapsed()){
        recButton->update();
    }

    if(this->inletsConnected[1] && *(float *)&_inletParams[1] == 1.0f){
        if(!recButton->getChecked()){
            recButton->setChecked(true);
            ofLog(OF_LOG_NOTICE,"START NDI VIDEO SENDER");
        }else{
            recButton->setChecked(false);
            ofLog(OF_LOG_NOTICE,"STOP NDI VIDEO SENDER");
        }

    }

}

//--------------------------------------------------------------
void VideoSender::drawObjectContent(ofxFontStash *font){
    ofSetColor(255);
    ofSetCircleResolution(50);
    ofEnableAlphaBlending();

    if(this->inletsConnected[0]){
        if(static_cast<ofTexture *>(_inletParams[0])->isAllocated()){
            if(!needToGrab){
                needToGrab = true;
                captureFbo.allocate(static_cast<ofTexture *>(_inletParams[0])->getWidth(), static_cast<ofTexture *>(_inletParams[0])->getHeight(), GL_RGB );
            }

            captureFbo.begin();
            ofClear(0,0,0,255);
            ofSetColor(255);
            static_cast<ofTexture *>(_inletParams[0])->draw(0,0,static_cast<ofTexture *>(_inletParams[0])->getWidth(),static_cast<ofTexture *>(_inletParams[0])->getHeight());
            captureFbo.end();

            if(recButton->getChecked()) {
                reader.readToPixels(captureFbo, capturePix,OF_IMAGE_COLOR); // ofxFastFboReader
                if(capturePix.getWidth() > 0 && capturePix.getHeight() > 0) {
                    ndiSender.send(capturePix);
                }
            }

        }
    }else{
        captureFbo.begin();
        ofClear(0,0,0,255);
        captureFbo.end();

        needToGrab = false;
    }

    if(static_cast<ofTexture *>(_inletParams[0])->isAllocated()){
        if(static_cast<ofTexture *>(_inletParams[0])->getWidth() >= static_cast<ofTexture *>(_inletParams[0])->getHeight()){   // horizontal texture
            drawW           = this->width;
            drawH           = (this->width/static_cast<ofTexture *>(_inletParams[0])->getWidth())*static_cast<ofTexture *>(_inletParams[0])->getHeight();
            posX            = 0;
            posY            = (this->height-drawH)/2.0f;
        }else{ // vertical texture
            drawW           = (static_cast<ofTexture *>(_inletParams[0])->getWidth()*this->height)/static_cast<ofTexture *>(_inletParams[0])->getHeight();
            drawH           = this->height;
            posX            = (this->width-drawW)/2.0f;
            posY            = 0;
        }
        captureFbo.getTexture().draw(posX,posY,drawW,drawH);
        if (recButton->getChecked()){
            ofSetColor(ofColor::red);
        }else{
            ofSetColor(ofColor::green);
        }
        ofDrawCircle(ofPoint(this->width-20, 30), 10);
    }
    gui->draw();
    ofDisableAlphaBlending();
}

//--------------------------------------------------------------
void VideoSender::removeObjectContent(bool removeFileFromData){

}

//--------------------------------------------------------------
void VideoSender::mouseMovedObjectContent(ofVec3f _m){
    gui->setCustomMousePos(static_cast<int>(_m.x - this->getPos().x),static_cast<int>(_m.y - this->getPos().y));
    header->setCustomMousePos(static_cast<int>(_m.x - this->getPos().x),static_cast<int>(_m.y - this->getPos().y));
    recButton->setCustomMousePos(static_cast<int>(_m.x - this->getPos().x),static_cast<int>(_m.y - this->getPos().y));
    if(!header->getIsCollapsed()){
        this->isOverGUI = header->hitTest(_m-this->getPos()) || recButton->hitTest(_m-this->getPos());

    }else{
        this->isOverGUI = header->hitTest(_m-this->getPos());
    }

}

//--------------------------------------------------------------
void VideoSender::dragGUIObject(ofVec3f _m){
    if(this->isOverGUI){
        gui->setCustomMousePos(static_cast<int>(_m.x - this->getPos().x),static_cast<int>(_m.y - this->getPos().y));
        header->setCustomMousePos(static_cast<int>(_m.x - this->getPos().x),static_cast<int>(_m.y - this->getPos().y));
        recButton->setCustomMousePos(static_cast<int>(_m.x - this->getPos().x),static_cast<int>(_m.y - this->getPos().y));
    }else{
        ofNotifyEvent(dragEvent, nId);

        box->setFromCenter(_m.x, _m.y,box->getWidth(),box->getHeight());
        headerBox->set(box->getPosition().x,box->getPosition().y,box->getWidth(),headerHeight);

        x = box->getPosition().x;
        y = box->getPosition().y;

        for(int j=0;j<static_cast<int>(outPut.size());j++){
            outPut[j]->linkVertices[0].move(outPut[j]->posFrom.x,outPut[j]->posFrom.y);
            outPut[j]->linkVertices[1].move(outPut[j]->posFrom.x+20,outPut[j]->posFrom.y);
        }
    }
}

//--------------------------------------------------------------
void VideoSender::onToggleEvent(ofxDatGuiToggleEvent e){
    if(!header->getIsCollapsed()){
        if(e.target == recButton){
            if(e.checked){
                ofLog(OF_LOG_NOTICE,"START NDI VIDEO SENDER");
            }else{
                ofLog(OF_LOG_NOTICE,"STOP NDI VIDEO SENDER");
            }
        }
    }
}

OBJECT_REGISTER( VideoSender, "video sender", OFXVP_OBJECT_CAT_VIDEO);

#endif
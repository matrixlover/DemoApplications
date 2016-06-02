#include "ofApp.h"

#define MOUSE_BUTTON_LEFT        0
#define MOUSE_BUTTON_MIDDLE      1
#define MOUSE_BUTTON_RIGHT       2

#define PANEL_WIDTH              450
#define PANEL_HEIGHT             700

#define STOVE_WIDTH              750
#define STOVE_HEIGHT             700

#define BORDER_WIDTH             20
#define BORDER_HEIGHT            5

//--------------------------------------------------------------
void ofApp::setup()
{
   ofPoint orig;
   
   ofEnableDepthTest();
   _light.setup();
   _light.setPosition(-100, 200,0);

   orig = ofPoint(ofGetWidth()/2-BORDER_WIDTH-STOVE_WIDTH,BORDER_HEIGHT-ofGetHeight()/2);
   _stove = Stove(orig, STOVE_WIDTH, STOVE_HEIGHT);

   orig = ofPoint(BORDER_WIDTH-ofGetWidth()/2,BORDER_HEIGHT-ofGetHeight()/2);
   _panel = AirPanel("Panel", orig, PANEL_WIDTH, PANEL_HEIGHT); 

   _bCamMouseEnabled = false;
}

//--------------------------------------------------------------
void ofApp::update()
{
   if (!_bCamMouseEnabled)
      _cam.reset();

   _stove.update();
   _panel.update();
}

//--------------------------------------------------------------
void ofApp::draw()
{	
   ofPoint m = ofPoint(mouseX-ofGetWidth()/2,ofGetHeight()/2-mouseY);

   _cam.begin();
   _cam.enableOrtho();
   ofRotateY(5);
   ofPushMatrix();

   ofBackground(0);

   // Draw Stove
   _stove.draw();
   
   for (int i=0; i<_stove.size(); i++)
   {
      Burner *b = _stove.getBurner(i);
      if (b)
      {
         if (b->isOver(m))
            b->select();
         else
            b->deselect();
      }
   }

   // Draw panel
   _panel.draw();

   for (int i=0; i<_panel.numButtons(); i++)
   {
      AirButton *button = _panel.getButton(i);
      if (button->isOver(m))
         button->select();
      else
         button->deselect();
   }
         
   ofPopMatrix();
   _cam.end();

}


//--------------------------------------------------------------
void ofApp::keyPressed(int key)
{
   switch (key)
   {
      case 'q':
         ofExit();
         break;

      case 'm':
         _bCamMouseEnabled = !_bCamMouseEnabled;
         break;

      default:
         break;
   }

   if (_panel.isSelected())
   {
      _panel.keyPressed(key);
   }
   else if (_stove.isSelected())
   {
      _stove.keyPressed(key);
   }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key)
{
   if (_panel.isSelected())
   {
      _panel.keyReleased(key);
   }
   else if (_stove.isSelected())
   {
      _stove.keyReleased(key);
   }
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y)
{
   ofPoint m = ofPoint(x-ofGetWidth()/2,ofGetHeight()/2-y);

   if (_panel.isSelected())
   {
      _panel.mouseMoved(m.x, m.y);
   }
   else if (_stove.isSelected())
   {
      _stove.mouseMoved(m.x, m.y);
   }
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button)
{
   ofPoint m = ofPoint(x-ofGetWidth()/2,ofGetHeight()/2-y);

   if (_panel.isSelected())
   {
      _panel.mouseDragged(m.x, m.y, button);
   }
   else if (_stove.isSelected())
   {
      _stove.mouseDragged(m.x, m.y, button);
   }
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{
   ofPoint m = ofPoint(x-ofGetWidth()/2,ofGetHeight()/2-y);

   if (_panel.isOver(m))
   {
      _panel.mousePressed(m.x, m.y, button);
   }
   else if (_stove.isOver(m))
   {
      _stove.mousePressed(m.x, m.y, button);
   }
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button)
{
   ofPoint m = ofPoint(x-ofGetWidth()/2,ofGetHeight()/2-y);

   if (_panel.isSelected() && _panel.isOver(m))
   {
      _panel.mouseReleased(m.x, m.y, button);
   }
   else if (_stove.isSelected() && _stove.isOver(m))
   {
      _stove.mouseReleased(m.x, m.y, button);
   }
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y)
{

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y)
{

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg)
{

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}

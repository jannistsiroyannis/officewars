#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/fetch.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include "client.h"
#include "rendering.h"
#include "statenav.h"

struct MouseState
{
   unsigned dragging;
   long lastTargetX;
   long lastTargetY;
} mouseState = {0};

struct ClientGame clientState;

EMSCRIPTEN_KEEPALIVE
unsigned getEdges(unsigned node, unsigned* outEdges)
{
   unsigned edgeCount = 0;
   getConnectedNodes(&clientState.state, node, clientState.edgeBuffer, &edgeCount);
   outEdges = clientState.edgeBuffer;
   return edgeCount;
}

static void step()
{
   static struct timespec last = {0};
   static unsigned cumulativeMs = 0; // May overflow, which is ok.
   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   unsigned deltaMs = (now.tv_sec - last.tv_sec) * 1000 + (now.tv_nsec - last.tv_nsec) / 1000000;
   last = now;
   cumulativeMs += deltaMs;

   if (mouseState.dragging)
   {
      clientState.viewEulerX += clientState.viewEulerXVel * deltaMs;
      clientState.viewEulerY += clientState.viewEulerYVel * deltaMs;
      if (clientState.viewEulerX > 2.0*M_PI)
	 clientState.viewEulerX -= 2.0*M_PI;
      if (clientState.viewEulerY > 2.0*M_PI)
	 clientState.viewEulerY -= 2.0*M_PI;
      if (clientState.viewEulerX < 0.0)
	 clientState.viewEulerX += 2.0*M_PI;
      if (clientState.viewEulerY < 0.0)
	 clientState.viewEulerY += 2.0*M_PI;

      clientState.viewEulerXVel *= 0.9;
      clientState.viewEulerYVel *= 0.9;
   }
   
   if (clientState.state.nodeCount == 0) // No game state yet (still waiting on server ?)
   {
      return;
   }

   Vec3 toSelected = V3ScalarMult(0.0016 * deltaMs,
				  V3Subt(V4toV3(clientState.state.nodeSpacePositions[clientState.nodeFocus]),
					 clientState.viewFocus));
   clientState.viewFocus = V3Add(clientState.viewFocus, toSelected);

   renderGraph(clientState.nodeFocus, clientState.viewEulerX, clientState.viewEulerY,
	       clientState.zoomFactor, clientState, UINT_MAX - cumulativeMs);
}

static void concludeAjaxRequest(emscripten_fetch_t *fetch)
{
   clientState.requestInFlight = 0; // open up for allowing new requests;
   
   //printf("Finished downloading %llu bytes from URL %s.\n", fetch->numBytes, fetch->url);
   //printf("Downloading %s failed, HTTP failure status code: %d.\n", fetch->url, fetch->status);

   if (fetch->status == 200)
   {
      if (clientState.requestCallback)
      {
	 (*clientState.requestCallback)(fetch->data, fetch->numBytes);
      }
   }
   else
   {
      printf("Request for \"%s\" failed with code: %d\n", fetch->url, fetch->status);
   }
   emscripten_fetch_close(fetch);
}

void makeAjaxRequest(const char* url, const char* method, const char* body,
		     void (*callback) (const char* data, unsigned size))
{
   // A request in flight already ? This super wierd behavior which seems to be taken for
   // granted within the JS-world, where requests are fired off concurrently on separate
   // connections, and received (both on other end and back here) in wildly different
   // order, at unpredictable times, if at all, when the state may be something else entirely
   // is just plain unacceptable. Here we limit oursevles to strictly one request at a time.
   // Replace with a queue if needed.
   if (clientState.requestInFlight)
   {
      printf("Request already in flight, ignore %s  %s\n", method, url);
      return;
   }

   clientState.requestInFlight = 1;

   clientState.requestCallback = callback;
   emscripten_fetch_attr_t attr;
   emscripten_fetch_attr_init(&attr);
   strcpy(attr.requestMethod, method);
   attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
   //attr.timeoutMSecs = 2000;
   attr.onsuccess = concludeAjaxRequest;
   attr.onerror = concludeAjaxRequest;

   if (body)
   {
      attr.requestDataSize = strlen(body);
      assert(attr.requestDataSize <= MAX_REQUEST_BODY_LEN);
      strcpy(clientState.requestBodyBuffer, body);
      // pointer must remain valid at least until emscripten_fetch_close (concludeAjaxRequest)
      attr.requestData = clientState.requestBodyBuffer;
   }
   else
   {
      attr.requestData = NULL;
      attr.requestDataSize = 0;
   }
   emscripten_fetch(&attr, url);
}

static unsigned getNodeNearestClick(Vec2 clickPos)
{
   float nearestSqrDist = 99999999.0;
   unsigned selectedNode = 0;
   for (unsigned i = 0; i < clientState.state.nodeCount; ++i)
   {
      Vec2 nodePos = V3toV2(clientState.nodeScreenPositions[i]);
      float sqrDist = V2SqrDist(nodePos, clickPos);
      if (sqrDist < nearestSqrDist)
      {
	 selectedNode = i;
	 nearestSqrDist = sqrDist;
      }
   }
   return selectedNode;
}

EM_BOOL mouseEnter(int eventType, const EmscriptenMouseEvent* mouseEvent, void* userData)
{
   mouseState.lastTargetX = mouseEvent->targetX;
   mouseState.lastTargetY = mouseEvent->targetY;
   return 1;
}

EM_BOOL mouseMove(int eventType, const EmscriptenMouseEvent* mouseEvent, void* userData)
{
   // Note to self: Buttons bit 0 = left, bit 1 = right, but this seems undocumented (and no defines in html5.h). Is it reliable?
   if (mouseEvent->buttons)
   {
      mouseState.dragging = 1;
   }
   
   if (mouseEvent->buttons & 0x1)
   {
      clientState.viewEulerXVel = ((float) (mouseEvent->targetY - mouseState.lastTargetY) * 0.0008);
      clientState.viewEulerYVel = ((float) (mouseEvent->targetX - mouseState.lastTargetX) * -0.0008);
   }
   mouseState.lastTargetX = mouseEvent->targetX;
   mouseState.lastTargetY = mouseEvent->targetY;
   return 1;
}

EM_JS(char*, getCookie, (const char* key),
      {
	 var gameId = UTF8ToString(key);
	 //console.log("looking for key: [" + gameId + "] in cookies: " + document.cookie);
	       
	 var cookieprops = document.cookie.split(";");
	 for(var i = 0; i < cookieprops.length; ++i)
	 {
	    var keyAndVal = cookieprops[i].split("=");
	    if (keyAndVal.length == 2 && keyAndVal[0].trim() == gameId.trim())
	    {
	       return allocate(intArrayFromString(keyAndVal[1]), ALLOC_NORMAL);
	    }
	 }
	 return 0;
      });

EM_JS(void, setCookie, (const char* key, const char* value),
      {
	 document.cookie = UTF8ToString(key) + "=" + UTF8ToString(value) + "; Max-Age=8640000; SameSite=Strict";
      });

EM_BOOL mouseUp(int eventType, const EmscriptenMouseEvent* mouseEvent, void* userData)
{
   if (!mouseState.dragging) // not dragging = a left click
   {
      Vec2 clickPos = { (float) mouseEvent->targetX, (float) mouseEvent->targetY };
      unsigned selectedNode = getNodeNearestClick(clickPos);
      
      if (mouseEvent->button == 0) // left button
      {
	 clientState.nodeFocus = selectedNode;
      } else if (mouseEvent->button == 2) // right button
      {
	 char orders[32];
	 char* playerSecret = getCookie(clientState.state.id);	 
	 if (playerSecret)
	 {
	    unsigned type = 0; // attack order
	    if (mouseEvent->ctrlKey)
	       type = 1; // support order
	    snprintf(orders, 32, "%u\n%u\n%u\n%s\n%s\n", type, clientState.nodeFocus, selectedNode,
		     clientState.state.id, playerSecret);
	    printf("Sent orders: %s\n", orders);
	    makeAjaxRequest("/cgi-bin/server/orders", "POST", orders, receiveState);
	    free(playerSecret);
	 }
      }
   }

   mouseState.dragging = 0;
   return 1;
}

EM_BOOL wheelMove(int eventType, const EmscriptenWheelEvent* wheelEvent, void *userData)
{
   switch (wheelEvent->deltaMode)
   {
      case DOM_DELTA_PIXEL:
	 clientState.zoomFactor *= (1.0 + 0.001 * wheelEvent->deltaY);
	 break;
      case DOM_DELTA_LINE:
	 clientState.zoomFactor *= (1.0 + 0.01 * wheelEvent->deltaY);
	 break;
      case DOM_DELTA_PAGE:
	 clientState.zoomFactor *= (1.0 + 0.1 * wheelEvent->deltaY);
	 break;
   }
   return 1;
}

int main(int argc, char** argv)
{
   // Setup mouse input callbacks
   const char* target = "#canv";
   EM_BOOL useCapture = 0;
   if ( emscripten_set_mouseenter_callback(target, NULL, useCapture, mouseEnter) < 0 ) { printf("Mouse callback failed!\n"); }
   if ( emscripten_set_mousemove_callback(target, NULL, useCapture, mouseMove) < 0 ) { printf("Mouse callback failed!\n"); }
   if ( emscripten_set_mouseup_callback(target, NULL, useCapture, mouseUp) < 0 ) { printf("Mouse callback failed!\n"); }
   if ( emscripten_set_wheel_callback(target, NULL, useCapture, wheelMove) < 0 ) { printf("Mouse callback failed!\n"); }

   clientState.nodeFocus = 0;
   clientState.zoomFactor = 1.0;
   clientState.state.nodeCount = 0;
   clientState.requestCallback = NULL;
   clientState.requestInFlight = 0;
   clientState.viewingTurn = 0;
   clientState.viewFocus.x = 0.0;
   clientState.viewFocus.y = 0.0;
   clientState.viewFocus.z = 0.0;
   
   printf("Requesting games list.\n");

   gotoState(GAME_SELECTION, 0);

   EM_ASM( start() );

   emscripten_set_main_loop(step, 0, 0);
}

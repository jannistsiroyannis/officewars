#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
#include <emscripten.h>
#include <emscripten/html5.h>

#include "../common/math/transforms.h"
#include "client.h"

#define NEAR_CLIP_Z 0.4
#define FAR_CLIP_Z 10000.0
#define FOV (70.0 * M_PI / 180.0)

EM_JS(int, getWidth, (),
      {
	 var canvas = document.getElementById("canv");
	 return canvas.width;
      });

EM_JS(int, getHeight, (),
      {
	 var canvas = document.getElementById("canv");
	 return canvas.height;
      });

void renderGraph(unsigned nodeFocus, float viewEulerX, float viewEulerY,
		 float zoomFactor, struct ClientGame clientState, unsigned dashOffset)
{
   float screenWidth = getWidth();
   float screenHeight = getHeight();

   // per node information
   Vec4* nodes = clientState.state.nodeSpacePositions;
   Vec3* transformed = clientState.nodeScreenPositions;
   
   unsigned* adjacencyMatrix = clientState.state.adjacencyMatrix;
   unsigned nodeCount = clientState.state.nodeCount;

   float aspect = screenWidth / screenHeight;
   setPerspectiveProjection(FOV, aspect, NEAR_CLIP_Z, FAR_CLIP_Z);

   // For simplicity, lets rotate the graph (model), instead of moving the POV (view) around..
   Frame graphPlacementFrame = getIdentityFrame();
   yawFrame(&graphPlacementFrame, -viewEulerY);
   pitchFrame(&graphPlacementFrame, viewEulerX);
   Vec3 back = {0.0, 0.0, -1.0};
   moveFrame(&graphPlacementFrame, back, 3.0*zoomFactor);
   setModelTransform(makeTransform(graphPlacementFrame));
   translate(V3ScalarMult(-1.0, clientState.viewFocus));
   
   setViewIdentity();
   //lookAt(eye, V4toV3(nodes[nodeFocus]), up);
   Mat4 modelMatrix = getModelMatrix();
   Mat4 viewMatrix = getViewMatrix();
   Mat4 projectionMatrix = getProjectionMatrix();
   Mat4 modelViewMatrix = M4multiply(&modelMatrix, &viewMatrix);
   Mat4 modelViewProjectionMatrix = M4multiply(&modelViewMatrix, &projectionMatrix);

   for (unsigned i = 0; i < nodeCount; ++i)
   {
      Vec4 p = M4V4Multiply(&modelViewProjectionMatrix, nodes[i]);
      p.x = p.x / p.w;
      p.y = p.y / p.w;
      p.z = p.z / p.w;
      // p now in NDC-space
      
      transformed[i] = viewportTransform(p, screenWidth, screenHeight, NEAR_CLIP_Z, FAR_CLIP_Z);
   }

   EM_ASM(
      {
	 var canvas = document.getElementById("canv");
	 var context = canvas.getContext("2d");   
	 var nodes = Module.HEAPF32.subarray($1/4, $1/4 + $0*3); // Float-array, xyz interleaved
	 var strength = Module.HEAPF32.subarray($3/4, $3/4 + $0);
         var orderCount = Module.HEAPU32.subarray($9/4, $9/4 + $0);
	 var controlledBy = Module.HEAPU32.subarray($4/4, $4/4 + $0);
	 //var homeWorld = Module.HEAPU32.subarray($9/4, $9/4 + $0);

	 var playerCount = $6;
	 var playerNames = Module.HEAPU32.subarray($7/4, $7/4 + $6); // array of player name char*
	 var playerColors = Module.HEAPU32.subarray($8/4, $8/4 + $6); // array of player color char*
	 
	 context.beginPath();
	 context.clearRect(0, 0, canvas.width, canvas.height);
	 context.font = "12px mono";
	 
	 var node;
	 for (node = 0; node < $0; node++)
	 {
	    var x1 = nodes[node*3+0];
	    var y1 = nodes[node*3+1];
	    var z1 = nodes[node*3+2];

	    // near/far clipping
	    if (z1 < 0.4 || z1 > 10000.0)
	    {
	       continue;
	    }

	    // Draw edges
	    var edgeCount = _getEdges(node, $2);
	    var edges = Module.HEAPU32.subarray($2/4, $2/4 + $0);
	    var edge;
	    context.lineWidth = "1";
	    context.setLineDash([]);
	    for (edge = 0; edge < edgeCount; edge++)
	    {
	       var connectedNode = edges[edge];
	       if (connectedNode > node) // We want to draw each edge only once, not back and forth
	       {
		  var x2 = nodes[connectedNode*3+0];
		  var y2 = nodes[connectedNode*3+1];
		  var z2 = nodes[connectedNode*3+2];

		  // near/far clipping
		  if (z2 < 0.4 || z2 > 10000.0)
		  {
		     continue;
		  }

		  context.beginPath();
		  context.moveTo(x1, y1);
		  context.lineTo(x2, y2);
		  if (node == $5 || connectedNode == $5)
		  {
		     context.strokeStyle = "black";
		  }
		  else
		  {
		     context.strokeStyle = "#c9c9c9";
		  }
		  context.fillStyle = "black";
		  context.stroke();
	       }
	    }

	    var perspectiveRadius = 5.0 / (z1 / 10000.0);

            if (node == $5) // View focus
	    {
	       context.beginPath();
	       context.lineWidth = "4";
	       context.strokeStyle = "black";
	       var offset = $15 * 0.005;
	       context.arc(nodes[node*3+0], nodes[node*3+1], perspectiveRadius+15, offset, offset + 0.3*Math.PI);
	       context.stroke();
	       context.beginPath();
	       context.arc(nodes[node*3+0], nodes[node*3+1], perspectiveRadius+15, offset + Math.PI, offset + 1.3*Math.PI);
	       context.stroke();
	    }
	    
	    // Draw occupying colors and names
	    if (controlledBy[node] != 4294967295) // 32bit -1 unsigned negative overflow :P
	    {
	       var controllingName = UTF8ToString(playerNames[controlledBy[node]]);
	       var controllingColor = UTF8ToString(playerColors[controlledBy[node]]); 
	       context.strokeStyle = controllingColor;
	       context.fillStyle = controllingColor;
               context.fillText("(" + node + ") " + controllingName, nodes[node*3+0]+5+perspectiveRadius, nodes[node*3+1]+perspectiveRadius);
               if (strength[node] != 1.0)
               {
                  if (orderCount[node] > 1)
                  {
                     context.fillText("+ " + orderCount[node] + " * " + strength[node].toFixed(2), nodes[node*3+0]+5+perspectiveRadius, nodes[node*3+1]+14+perspectiveRadius);
                  }
                  else
                  {
                     context.fillText("+ " + strength[node].toFixed(2), nodes[node*3+0]+5+perspectiveRadius, nodes[node*3+1]+14+perspectiveRadius);
                  }
               }
	       context.fillStyle = controllingColor;
	    }
	    else
	    {
	       context.fillStyle = "black";
               context.fillText("(" + node + ")", nodes[node*3+0]+5+perspectiveRadius, nodes[node*3+1]+5+perspectiveRadius);
	    }

	    // Draw the node itself
	    context.beginPath();
	    context.arc(nodes[node*3+0], nodes[node*3+1], perspectiveRadius, 0, 2*Math.PI);
	    context.closePath();
	    context.fill();
	 }

	 var orderCount = $10;
	 var orderPlayers = Module.HEAPU32.subarray($11/4, $11/4 + $10);
	 var orderFromNodes = Module.HEAPU32.subarray($12/4, $12/4 + $10);
	 var orderToNodes = Module.HEAPU32.subarray($13/4, $13/4 + $10);
	 var orderTypes = Module.HEAP32.subarray($14/4, $14/4 + $10);

	 //console.log("will now render " + orderCount + " orders.");
	 
	 var order;
	 for (order = 0; order < orderCount; order++)
	 {
            // Surrender orders are not renderd
            if (orderTypes[order] == 2)
               continue;
            
	    var x1 = nodes[orderFromNodes[order]*3+0];
	    var y1 = nodes[orderFromNodes[order]*3+1];
	    var z1 = nodes[orderFromNodes[order]*3+2];

	    var x2 = nodes[orderToNodes[order]*3+0];
	    var y2 = nodes[orderToNodes[order]*3+1];
	    var z2 = nodes[orderToNodes[order]*3+2];

	    // near/far clipping
	    if (z1 < 0.4 || z1 > 10000.0 || z2 < 0.4 || z2 > 10000.0)
	    {
	       continue;
	    }

	    var xToT = x2 - x1;
	    var yToT = y2 - y1;
	    var norm = Math.sqrt(xToT*xToT + yToT*yToT);
	    xToT /= norm;
	    yToT /= norm;

	    var perpX = -yToT;
	    var perpY = xToT;

	    var controllingColor = UTF8ToString(playerColors[orderPlayers[order]]);
	    var perspectiveRadius1 = 5.0 / (z1 / 10000.0);
	    var perspectiveRadius2 = 5.0 / (z2 / 10000.0);
	    context.strokeStyle = controllingColor;
	    if (orderTypes[order] == 0)
	    {
	       context.setLineDash([15, 5]);
	       context.lineWidth = "3";
	    
	    }
	    else
	    {
	       context.setLineDash([5, 15]);
	       context.lineWidth = "2";
	    }
	    context.lineDashOffset = $15 / 100;
	    context.beginPath();
	    context.moveTo(x1+perpX*perspectiveRadius1, y1+perpY*perspectiveRadius1);
	    context.lineTo(x2+perpX*perspectiveRadius2, y2+perpY*perspectiveRadius2);
	    context.stroke();

	    //context.fillText("Order from here: " + order, x1+8, y1-8);
	    //context.fillText("Order to here: " + order, x2+8, y2-8);
	 }
	 
      },
      nodeCount, // $0
      (float*) transformed, // $1
      clientState.edgeBuffer, // $2
      clientState.supportBuffer, // $3
      clientState.state.controlledBy, // $4
      nodeFocus, // $5
      clientState.state.playerCount, // $6
      clientState.state.playerName, // $7
      clientState.state.playerColor, // $8
      clientState.orderCountBuffer, // $9
      clientState.state.turn[clientState.viewingTurn].orderCount, // $10
      clientState.state.turn[clientState.viewingTurn].issuingPlayer, // $11
      clientState.state.turn[clientState.viewingTurn].fromNode, // $12
      clientState.state.turn[clientState.viewingTurn].toNode, // $13
      clientState.state.turn[clientState.viewingTurn].type, // $14
      dashOffset // $15
   );

}

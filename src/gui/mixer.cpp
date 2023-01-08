/**
 * Furnace Tracker - multi-system chiptune tracker
 * Copyright (C) 2021-2022 tildearrow and contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "gui.h"
#include "intConst.h"
#include <fmt/printf.h>
#include "imgui_internal.h"

bool FurnaceGUI::portSet(String label, unsigned int portSetID, int ins, int outs) {
  String portID=fmt::sprintf("portSet%.4x",portSetID);

  ImDrawList* dl=ImGui::GetWindowDrawList();
  ImGuiWindow* window=ImGui::GetCurrentWindow();
  ImGuiStyle& style=ImGui::GetStyle();

  ImVec2 labelSize=ImGui::CalcTextSize(label.c_str());

  ImVec2 size=labelSize;

  // pad
  size.x+=style.FramePadding.x*2.0f;
  size.y+=style.FramePadding.y*2.0f;

  // space for ports
  size.y+=MAX(ins,outs)*labelSize.y+style.FramePadding.y*2.0f+style.ItemSpacing.y;

  ImVec4 portSetBorderColor=uiColors[GUI_COLOR_PATCHBAY_PORTSET];
  ImVec4 portSetColor=ImVec4(
    portSetBorderColor.x*0.75f,
    portSetBorderColor.y*0.75f,
    portSetBorderColor.z*0.75f,
    portSetBorderColor.w
  );

  ImVec4 portBorderColor=uiColors[GUI_COLOR_PATCHBAY_PORT];
  ImVec4 portColor=ImVec4(
    portBorderColor.x*0.75f,
    portBorderColor.y*0.75f,
    portBorderColor.z*0.75f,
    portBorderColor.w
  );

  ImVec2 minArea=window->DC.CursorPos;
  ImVec2 maxArea=ImVec2(
    minArea.x+size.x,
    minArea.y+size.y
  );
  ImRect rect=ImRect(minArea,maxArea);

  ImVec2 textPos=ImVec2(
    minArea.x+style.FramePadding.x,
    minArea.y+style.FramePadding.y
  );

  ImGui::ItemSize(size,style.FramePadding.y);
  if (ImGui::ItemAdd(rect,ImGui::GetID(portID.c_str()))) {
    // label
    dl->AddRectFilled(minArea,maxArea,ImGui::GetColorU32(portSetColor),0.0f);
    dl->AddRect(minArea,maxArea,ImGui::GetColorU32(portSetBorderColor),0.0f,dpiScale);
    dl->AddText(textPos,ImGui::GetColorU32(uiColors[GUI_COLOR_TEXT]),label.c_str());

    // ports
    for (int i=0; i<outs; i++) {
      String portLabel=fmt::sprintf("%d",i+1);
      ImVec2 portLabelSize=ImGui::CalcTextSize(portLabel.c_str());

      ImVec2 portMin=ImVec2(
        maxArea.x-portLabelSize.x-style.FramePadding.x*2,
        minArea.y+style.FramePadding.y+labelSize.y+(style.ItemSpacing.y+portLabelSize.y+style.FramePadding.y*2)*i
      );
      ImVec2 portMax=ImVec2(
        maxArea.x,
        minArea.y+style.FramePadding.y+labelSize.y+portLabelSize.y+style.FramePadding.y*2+(style.ItemSpacing.y+portLabelSize.y+style.FramePadding.y*2)*i
      );
      ImVec2 portLabelPos=portMin;
      portLabelPos.x+=style.FramePadding.x;
      portLabelPos.y+=style.FramePadding.y;

      dl->AddRectFilled(portMin,portMax,ImGui::GetColorU32(portColor),0.0f);
      dl->AddRect(portMin,portMax,ImGui::GetColorU32(portBorderColor),0.0f,dpiScale);
      dl->AddText(portLabelPos,ImGui::GetColorU32(uiColors[GUI_COLOR_TEXT]),portLabel.c_str());
    }
  }

  return false;
}

void FurnaceGUI::drawMixer() {
  if (nextWindow==GUI_WINDOW_MIXER) {
    mixerOpen=true;
    ImGui::SetNextWindowFocus();
    nextWindow=GUI_WINDOW_NOTHING;
  }
  if (!mixerOpen) return;
  if (mobileUI) {
    patWindowPos=(portrait?ImVec2(0.0f,(mobileMenuPos*-0.65*canvasH)):ImVec2((0.16*canvasH)+0.5*canvasW*mobileMenuPos,0.0f));
    patWindowSize=(portrait?ImVec2(canvasW,canvasH-(0.16*canvasW)):ImVec2(canvasW-(0.16*canvasH),canvasH));
    ImGui::SetNextWindowPos(patWindowPos);
    ImGui::SetNextWindowSize(patWindowSize);
  } else {
    ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f*dpiScale,200.0f*dpiScale),ImVec2(canvasW,canvasH));
  }
  if (ImGui::Begin("Mixer",&mixerOpen,globalWinFlags|(settings.allowEditDocking?0:ImGuiWindowFlags_NoDocking))) {
    if (ImGui::SliderFloat("Master Volume",&e->song.masterVol,0,3,"%.2fx")) {
      if (e->song.masterVol<0) e->song.masterVol=0;
      if (e->song.masterVol>3) e->song.masterVol=3;
      MARK_MODIFIED;
    } rightClickable
    if (selectedPortSet<e->song.systemLen) {
      if (ImGui::BeginTable("CurPortSet",2)) {
        ImGui::TableSetupColumn("c0",ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("c1",ImGuiTableColumnFlags_WidthFixed);

        bool doInvert=e->song.systemVol[selectedPortSet]<0;
        float vol=fabs(e->song.systemVol[selectedPortSet]);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%d. %s",selectedPortSet+1,getSystemName(e->song.system[selectedPortSet]));
        ImGui::TableNextColumn();
        if (ImGui::Checkbox("Invert",&doInvert)) {
          e->song.systemVol[selectedPortSet]=-e->song.systemVol[selectedPortSet];
          MARK_MODIFIED;
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (CWSliderFloat("##Volume",&vol,0,2)) {
          if (doInvert) {
            if (vol<0.0001) vol=0.0001;
          }
          if (vol<0) vol=0;
          if (vol>10) vol=10;
          e->song.systemVol[selectedPortSet]=(doInvert)?-vol:vol;
          MARK_MODIFIED;
        } rightClickable
        ImGui::TableNextColumn();
        ImGui::Text("Volume");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (CWSliderFloat("##Panning",&e->song.systemPan[selectedPortSet],-1.0f,1.0f)) {
          if (e->song.systemPan[selectedPortSet]<-1.0f) e->song.systemPan[selectedPortSet]=-1.0f;
          if (e->song.systemPan[selectedPortSet]>1.0f) e->song.systemPan[selectedPortSet]=1.0f;
          MARK_MODIFIED;
        } rightClickable
        ImGui::TableNextColumn();
        ImGui::Text("Panning");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (CWSliderFloat("##FrontRear",&e->song.systemPanFR[selectedPortSet],-1.0f,1.0f)) {
          if (e->song.systemPanFR[selectedPortSet]<-1.0f) e->song.systemPanFR[selectedPortSet]=-1.0f;
          if (e->song.systemPanFR[selectedPortSet]>1.0f) e->song.systemPanFR[selectedPortSet]=1.0f;
          MARK_MODIFIED;
        } rightClickable
        ImGui::TableNextColumn();
        ImGui::Text("Front/Rear");

        ImGui::EndTable();
      }
    }
    if (ImGui::BeginChild("Patchbay",ImVec2(0,0),true)) {
      ImGui::Text("The patchbay goes here...");
      for (int i=0; i<e->song.systemLen; i++) {
        DivDispatch* dispatch=e->getDispatch(i);
        if (dispatch==NULL) continue;
        if (portSet(fmt::sprintf("%d. %s",i+1,getSystemName(e->song.system[i])),i,0,dispatch->getOutputCount())) {
          selectedPortSet=i;
        }
      }
    }
    ImGui::EndChild();
  }
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) curWindow=GUI_WINDOW_MIXER;
  ImGui::End();
}

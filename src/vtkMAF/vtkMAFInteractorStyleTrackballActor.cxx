/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMAFInteractorStyleTrackballActor.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkMAFInteractorStyleTrackballActor.h"

#include <vtkCamera.h>
#include <vtkCellPicker.h>
#include <vtkCallbackCommand.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkObjectFactory.h>
#include <vtkAssembly.h>
#include <vtkAssemblyPath.h>
#include <vtkProp3D.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkRenderWindow.h>
#include <vtkTransform.h>
#include <vtkProp3DCollection.h>
#include <vtkProperty.h>

vtkStandardNewMacro(vtkMAFInteractorStyleTrackballActor);

//----------------------------------------------------------------------------
vtkMAFInteractorStyleTrackballActor::vtkMAFInteractorStyleTrackballActor() 
{
  this->MotionFactor    = 10.0;
  this->InteractionAssembly = NULL;
  this->DefaultRenderer = NULL;
  this->InteractionPicker = vtkCellPicker::New();
  this->InteractionPicker->SetTolerance(0.001);
}

//----------------------------------------------------------------------------
vtkMAFInteractorStyleTrackballActor::~vtkMAFInteractorStyleTrackballActor() 
{
    SetDefaultRenderer(NULL);
    this->InteractionPicker->Delete();
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::SetDefaultRenderer(vtkRenderer *ren) {
    if (ren != this->DefaultRenderer) {
        if (this->DefaultRenderer)
        {
            this->DefaultRenderer->UnRegister(NULL);
        }
        this->DefaultRenderer = ren;
        if (this->DefaultRenderer)
        {
            this->DefaultRenderer->Register(NULL);
        }
        this->SetCurrentRenderer(DefaultRenderer);
    }
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::FindInteractiveRenderer(int x, int y) {
    vtkRendererCollection *rc;
    vtkRenderer *ren;
    rc = this->Interactor->GetRenderWindow()->GetRenderers();
    rc->InitTraversal();
    while(ren = rc->GetNextItem()) {
        if (ren->GetInteractive()) {
            if (this->InteractionPicker->Pick(x, y, 0.0, ren) != 0) {
                SetCurrentRenderer(ren);
                return;
            }
        }
    }
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::OnMouseMove() 
{
    switch (this->State) 
    {
    case VTKIS_ROTATE:
      this->Rotate();
      this->InvokeEvent(vtkCommand::InteractionEvent, NULL);
      break;

    case VTKIS_PAN:
      this->Pan();
      this->InvokeEvent(vtkCommand::InteractionEvent, NULL);
      break;

    case VTKIS_DOLLY:
      this->Dolly();
      this->InvokeEvent(vtkCommand::InteractionEvent, NULL);
      break;

    case VTKIS_SPIN:
      this->Spin();
      this->InvokeEvent(vtkCommand::InteractionEvent, NULL);
      break;

    case VTKIS_USCALE:
      this->UniformScale();
      this->InvokeEvent(vtkCommand::InteractionEvent, NULL);
      break;
    }
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::OnLeftButtonDown() 
{
  int x = this->Interactor->GetEventPosition()[0];
  int y = this->Interactor->GetEventPosition()[1];

  this->FindInteractiveRenderer(x, y);
  this->FindPickedActor(x, y);
  if (this->CurrentRenderer == NULL || this->InteractionAssembly == NULL)
    {
    return;
    }

  this->GrabFocus(this->EventCallbackCommand);
  if (this->Interactor->GetShiftKey())
    {
    this->StartPan();
    }
  else if (this->Interactor->GetControlKey())
    {
    this->StartSpin();
    }
  else
    {
    this->StartRotate();
    }
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::OnLeftButtonUp()
{
  switch (this->State) 
    {
    case VTKIS_PAN:
      this->EndPan();
      break;

    case VTKIS_SPIN:
      this->EndSpin();
      break;

    case VTKIS_ROTATE:
      this->EndRotate();
      break;
    }

  if ( this->Interactor )
    {
    this->ReleaseFocus();
    }
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::OnMiddleButtonDown() 
{
  int x = this->Interactor->GetEventPosition()[0];
  int y = this->Interactor->GetEventPosition()[1];

  this->FindInteractiveRenderer(x, y);
  this->FindPickedActor(x, y);
  if (this->CurrentRenderer == NULL || this->InteractionAssembly == NULL)
    {
    return;
    }

  this->GrabFocus(this->EventCallbackCommand);
  if (this->Interactor->GetControlKey())
    {
    this->StartDolly();
    }
  else
    {
    this->StartPan();
    }
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::OnMiddleButtonUp()
{
  switch (this->State) 
    {
    case VTKIS_DOLLY:
      this->EndDolly();
      break;

    case VTKIS_PAN:
      this->EndPan();
      break;
    }

  if ( this->Interactor )
    {
    this->ReleaseFocus();
    }
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::OnRightButtonDown() 
{
  int x = this->Interactor->GetEventPosition()[0];
  int y = this->Interactor->GetEventPosition()[1];

  this->FindInteractiveRenderer(x, y);
  this->FindPickedActor(x, y);
  if (this->CurrentRenderer == NULL || this->InteractionAssembly == NULL)
    {
    return;
    }
  
  this->GrabFocus(this->EventCallbackCommand);
  this->StartUniformScale();
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::OnRightButtonUp()
{
  switch (this->State) 
    {
    case VTKIS_USCALE:
      this->EndUniformScale();
      break;
    }

  if ( this->Interactor )
    {
    this->ReleaseFocus();
    }
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::Rotate()
{
  if (this->CurrentRenderer == NULL || this->InteractionAssembly == NULL)
    {
    return;
    }
  
  vtkRenderWindowInteractor *rwi = this->Interactor;
  vtkCamera *cam = this->CurrentRenderer->GetActiveCamera();

  // First get the origin of the assembly
  double *obj_center = this->InteractionAssembly->GetCenter();
  
  // GetLength gets the length of the diagonal of the bounding box
  double boundRadius = this->InteractionAssembly->GetLength() * 0.5;

  // Get the view up and view right vectors
  double view_up[3], view_look[3], view_right[3];

  cam->OrthogonalizeViewUp();
  cam->ComputeViewPlaneNormal();
  cam->GetViewUp(view_up);
  vtkMath::Normalize(view_up);
  cam->GetViewPlaneNormal(view_look);
  vtkMath::Cross(view_up, view_look, view_right);
  vtkMath::Normalize(view_right);
  
  // Get the furtherest point from object position+origin
  double outsidept[3];

  outsidept[0] = obj_center[0] + view_right[0] * boundRadius;
  outsidept[1] = obj_center[1] + view_right[1] * boundRadius;
  outsidept[2] = obj_center[2] + view_right[2] * boundRadius;
  
  // Convert them to display coord
  double disp_obj_center[3];

  this->ComputeWorldToDisplay(obj_center[0], obj_center[1], obj_center[2], 
                              disp_obj_center);

  this->ComputeWorldToDisplay(outsidept[0], outsidept[1], outsidept[2], 
                              outsidept);
  
  double radius = sqrt(vtkMath::Distance2BetweenPoints(disp_obj_center,
                                                       outsidept));
  double nxf = (rwi->GetEventPosition()[0] - disp_obj_center[0]) / radius;

  double nyf = (rwi->GetEventPosition()[1] - disp_obj_center[1]) / radius;

  double oxf = (rwi->GetLastEventPosition()[0] - disp_obj_center[0]) / radius;

  double oyf = (rwi->GetLastEventPosition()[1] - disp_obj_center[1]) / radius;

  if (((nxf * nxf + nyf * nyf) <= 1.0) &&
      ((oxf * oxf + oyf * oyf) <= 1.0))
    {
    double newXAngle = vtkMath::DegreesFromRadians( asin( nxf ) );
    double newYAngle = vtkMath::DegreesFromRadians( asin( nyf ) );
    double oldXAngle = vtkMath::DegreesFromRadians( asin( oxf ) );
    double oldYAngle = vtkMath::DegreesFromRadians( asin( oyf ) );
    
    double scale[3];
    scale[0] = scale[1] = scale[2] = 1.0;

    double **rotate = new double*[2];

    rotate[0] = new double[4];
    rotate[1] = new double[4];
    
    rotate[0][0] = newXAngle - oldXAngle;
    rotate[0][1] = view_up[0];
    rotate[0][2] = view_up[1];
    rotate[0][3] = view_up[2];
    
    rotate[1][0] = oldYAngle - newYAngle;
    rotate[1][1] = view_right[0];
    rotate[1][2] = view_right[1];
    rotate[1][3] = view_right[2];
    
    
    this->Prop3DTransform(this->InteractionAssembly,
                          obj_center,
                          2, 
                          rotate, 
                          scale);
    
    delete [] rotate[0];
    delete [] rotate[1];
    delete [] rotate;
    
    if (this->AutoAdjustCameraClippingRange)
      {
      this->CurrentRenderer->ResetCameraClippingRange();
      }

    rwi->Render();
    }
}
  
//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::Spin()
{
  if ( this->CurrentRenderer == NULL || this->InteractionAssembly == NULL )
    {
    return;
    }

  vtkRenderWindowInteractor *rwi = this->Interactor;
  vtkCamera *cam = this->CurrentRenderer->GetActiveCamera();
  
  // Get the axis to rotate around = vector from eye to origin

  double *obj_center = this->InteractionAssembly->GetCenter();

  double motion_vector[3];
  double view_point[3];

  if (cam->GetParallelProjection())
    {
    // If parallel projection, want to get the view plane normal...
    cam->ComputeViewPlaneNormal();
    cam->GetViewPlaneNormal( motion_vector );
    }
  else
    {   
    // Perspective projection, get vector from eye to center of actor
    cam->GetPosition( view_point );
    motion_vector[0] = view_point[0] - obj_center[0];
    motion_vector[1] = view_point[1] - obj_center[1];
    motion_vector[2] = view_point[2] - obj_center[2];
    vtkMath::Normalize(motion_vector);
    }
  
  double disp_obj_center[3];
  
  this->ComputeWorldToDisplay(obj_center[0], obj_center[1], obj_center[2], 
                              disp_obj_center);
  
  double newAngle = 
    vtkMath::DegreesFromRadians( atan2( rwi->GetEventPosition()[1] - disp_obj_center[1],
                                        rwi->GetEventPosition()[0] - disp_obj_center[0] ) );

  double oldAngle = 
    vtkMath::DegreesFromRadians( atan2( rwi->GetLastEventPosition()[1] - disp_obj_center[1],
                                        rwi->GetLastEventPosition()[0] - disp_obj_center[0] ) );
  
  double scale[3];
  scale[0] = scale[1] = scale[2] = 1.0;

  double **rotate = new double*[1];
  rotate[0] = new double[4];
  
  rotate[0][0] = newAngle - oldAngle;
  rotate[0][1] = motion_vector[0];
  rotate[0][2] = motion_vector[1];
  rotate[0][3] = motion_vector[2];
  
  this->Prop3DTransform( this->InteractionAssembly,
                         obj_center,
                         1, 
                         rotate, 
                         scale );
  
  delete [] rotate[0];
  delete [] rotate;
  
  if ( this->AutoAdjustCameraClippingRange )
    {
    this->CurrentRenderer->ResetCameraClippingRange();
    }

  rwi->Render();
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::Pan()
{
  if (this->CurrentRenderer == NULL || this->InteractionAssembly == NULL)
    {
    return;
    }

  vtkRenderWindowInteractor *rwi = this->Interactor;
  
  // Use initial center as the origin from which to pan

  double *obj_center = this->InteractionAssembly->GetCenter();

  double disp_obj_center[3], new_pick_point[4];
  double old_pick_point[4], motion_vector[3];
  
  this->ComputeWorldToDisplay(obj_center[0], obj_center[1], obj_center[2], 
                              disp_obj_center);

  this->ComputeDisplayToWorld(rwi->GetEventPosition()[0], 
                              rwi->GetEventPosition()[1], 
                              disp_obj_center[2],
                              new_pick_point);
  
  this->ComputeDisplayToWorld(rwi->GetLastEventPosition()[0], 
                              rwi->GetLastEventPosition()[1], 
                              disp_obj_center[2],
                              old_pick_point);
  
  motion_vector[0] = new_pick_point[0] - old_pick_point[0];
  motion_vector[1] = new_pick_point[1] - old_pick_point[1];
  motion_vector[2] = new_pick_point[2] - old_pick_point[2];
  
  if (this->InteractionAssembly->GetUserMatrix() != NULL)
    {
    vtkTransform *t = vtkTransform::New();
    t->PostMultiply();
    t->SetMatrix(this->InteractionAssembly->GetUserMatrix());
    t->Translate(motion_vector[0], motion_vector[1], motion_vector[2]);
    this->InteractionAssembly->GetUserMatrix()->DeepCopy(t->GetMatrix());
    t->Delete();
    }
  else
    {
    this->InteractionAssembly->AddPosition(motion_vector[0],
                                       motion_vector[1],
                                       motion_vector[2]);
    }
  
  if (this->AutoAdjustCameraClippingRange)
    {
    this->CurrentRenderer->ResetCameraClippingRange();
    }

  rwi->Render();
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::Dolly()
{
  if (this->CurrentRenderer == NULL || this->InteractionAssembly == NULL)
    {
    return;
    }
  
  vtkRenderWindowInteractor *rwi = this->Interactor;
  vtkCamera *cam = this->CurrentRenderer->GetActiveCamera();

  double view_point[3], view_focus[3];
  double motion_vector[3];

  cam->GetPosition(view_point);
  cam->GetFocalPoint(view_focus);
  
  double *center = this->CurrentRenderer->GetCenter();

  int dy = rwi->GetEventPosition()[1] - rwi->GetLastEventPosition()[1];
  double yf = dy / center[1] * this->MotionFactor;
  double dollyFactor = pow(1.1, yf);
  
  dollyFactor -= 1.0;
  motion_vector[0] = (view_point[0] - view_focus[0]) * dollyFactor;
  motion_vector[1] = (view_point[1] - view_focus[1]) * dollyFactor;
  motion_vector[2] = (view_point[2] - view_focus[2]) * dollyFactor;
  
  if (this->InteractionAssembly->GetUserMatrix() != NULL)
    {
    vtkTransform *t = vtkTransform::New();
    t->PostMultiply();
    t->SetMatrix(this->InteractionAssembly->GetUserMatrix());
    t->Translate(motion_vector[0], motion_vector[1], 
                 motion_vector[2]);
    this->InteractionAssembly->GetUserMatrix()->DeepCopy(t->GetMatrix());
    t->Delete();
    }
  else
    {
    this->InteractionAssembly->AddPosition(motion_vector);
    }
  
  if (this->AutoAdjustCameraClippingRange)
    {
    this->CurrentRenderer->ResetCameraClippingRange();
    }

  rwi->Render();
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::UniformScale()
{
  if (this->CurrentRenderer == NULL || this->InteractionAssembly == NULL)
    {
    return;
    }
  
  vtkRenderWindowInteractor *rwi = this->Interactor;

  int dy = rwi->GetEventPosition()[1] - rwi->GetLastEventPosition()[1];
 
  double *obj_center = this->InteractionAssembly->GetCenter();
  double *center = this->CurrentRenderer->GetCenter();

  double yf = dy / center[1] * this->MotionFactor;
  double scaleFactor = pow(1.1, yf);
  
  double **rotate = NULL;
  
  double scale[3];
  scale[0] = scale[1] = scale[2] = scaleFactor;
  
  this->Prop3DTransform(this->InteractionAssembly,
                        obj_center,
                        0, 
                        rotate, 
                        scale);
  
  if (this->AutoAdjustCameraClippingRange)
    {
    this->CurrentRenderer->ResetCameraClippingRange();
    }

  rwi->Render();
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::FindPickedActor(int x, int y)
{
    if(this->InteractionPicker->Pick(x, y, 0.0, this->CurrentRenderer) != 0) {
        vtkAssemblyPath *path = this->InteractionPicker->GetPath(); //change how retrieve prop
        if (path) {
            vtkProp *prop = path->GetLastNode()->GetViewProp(); //

            vtkAssemblyNode *itemP;
            int num = path->GetNumberOfItems();
            path->InitTraversal();
            itemP = path->GetNextNode();

            this->InteractionAssembly = NULL;
            while(itemP->GetViewProp()->IsA("vtkAssembly")) {
                this->InteractionAssembly = vtkAssembly::SafeDownCast(itemP->GetViewProp());
                itemP = path->GetNextNode();        
            }
        }
    }
}

//----------------------------------------------------------------------------
void vtkMAFInteractorStyleTrackballActor::Prop3DTransform(vtkProp3D *prop3D,
                                                       double *boxCenter,
                                                       int numRotation,
                                                       double **rotate,
                                                       double *scale)
{
  vtkMatrix4x4 *oldMatrix = vtkMatrix4x4::New();
  prop3D->GetMatrix(oldMatrix);
  
  double orig[3];
  prop3D->GetOrigin(orig);
  
  vtkTransform *newTransform = vtkTransform::New();
  newTransform->PostMultiply();
  if (prop3D->GetUserMatrix() != NULL) 
    {
    newTransform->SetMatrix(prop3D->GetUserMatrix());
    }
  else 
    {
    newTransform->SetMatrix(oldMatrix);
    }
  
  newTransform->Translate(-(boxCenter[0]), -(boxCenter[1]), -(boxCenter[2]));
  
  for (int i = 0; i < numRotation; i++) 
    {
    newTransform->RotateWXYZ(rotate[i][0], rotate[i][1],
                             rotate[i][2], rotate[i][3]);
    }
  
  if ((scale[0] * scale[1] * scale[2]) != 0.0) 
    {
    newTransform->Scale(scale[0], scale[1], scale[2]);
    }
  
  newTransform->Translate(boxCenter[0], boxCenter[1], boxCenter[2]);
  
  // now try to get the composite of translate, rotate, and scale
  newTransform->Translate(-(orig[0]), -(orig[1]), -(orig[2]));
  newTransform->PreMultiply();
  newTransform->Translate(orig[0], orig[1], orig[2]);
  
  if (prop3D->GetUserMatrix() != NULL) 
    {
    newTransform->GetMatrix(prop3D->GetUserMatrix());
    }
  else 
    {
    prop3D->SetPosition(newTransform->GetPosition());
    prop3D->SetScale(newTransform->GetScale());
    prop3D->SetOrientation(newTransform->GetOrientation());
    }
  oldMatrix->Delete();
  newTransform->Delete();
}

vtkAssembly *vtkMAFInteractorStyleTrackballActor::GetInteractionAssembly() {
    return InteractionAssembly;
}

void vtkMAFInteractorStyleTrackballActor::OnChar() 
{
    vtkRenderWindowInteractor *rwi = this->Interactor;

    switch (rwi->GetKeyCode()) 
    {
    case 'm' :
    case 'M' :
        if (this->AnimState == VTKIS_ANIM_OFF) 
        {
            this->StartAnimate();
        }
        else 
        {
            this->StopAnimate();
        }
        break;

    case 'Q' :
    case 'q' :
    case 'e' :
    case 'E' :
        break;

    case 'f' :      
    case 'F' :
        {
            if(this->CurrentRenderer!=0)
            {
                this->AnimState = VTKIS_ANIM_ON;
                vtkAssemblyPath *path = NULL;
                this->FindInteractiveRenderer(rwi->GetEventPosition()[0],
                    rwi->GetEventPosition()[1]);
                rwi->GetPicker()->Pick(rwi->GetEventPosition()[0],
                    rwi->GetEventPosition()[1],
                    0.0,
                    this->CurrentRenderer);
                vtkAbstractPropPicker *picker;
                if ((picker=vtkAbstractPropPicker::SafeDownCast(rwi->GetPicker())))
                {
                    path = picker->GetPath();
                }
                if (path != NULL)
                {
                    rwi->FlyTo(this->CurrentRenderer, picker->GetPickPosition());
                }
                this->AnimState = VTKIS_ANIM_OFF;
            }
            else
            {
                vtkWarningMacro(<<"no current renderer on the interactor style.");
            }
        }
        break;

    case 'u' :
    case 'U' :
        {
            rwi->UserCallback();
        }
        break;

    case 'r' :
    case 'R' :
        {
            this->CurrentRenderer->ResetCamera();
            this->CurrentRenderer->GetRenderWindow()->Render();
        }
        break;

    case 'w' :
    case 'W' :
        {
            vtkActorCollection *ac;
            vtkActor *anActor, *aPart;
            vtkAssemblyPath *path;
            this->FindInteractiveRenderer(rwi->GetEventPosition()[0],
                rwi->GetEventPosition()[1]);
            if(this->CurrentRenderer!=0)
            {
                ac = this->CurrentRenderer->GetActors();
                vtkCollectionSimpleIterator ait;
                for (ac->InitTraversal(ait); (anActor = ac->GetNextActor(ait)); )
                {
                    for (anActor->InitPathTraversal(); (path=anActor->GetNextPath()); )
                    {
                        aPart=static_cast<vtkActor *>(path->GetLastNode()->GetViewProp());
                        aPart->GetProperty()->SetRepresentationToWireframe();
                    }
                }
            }
            else
            {
                vtkWarningMacro(<<"no current renderer on the interactor style.");
            }
            rwi->Render();
        }
        break;

    case 's' :
    case 'S' :
        {
            vtkActorCollection *ac;
            vtkActor *anActor, *aPart;
            vtkAssemblyPath *path;
            this->FindInteractiveRenderer(rwi->GetEventPosition()[0],
                rwi->GetEventPosition()[1]);
            if(this->CurrentRenderer!=0)
            {
                ac = this->CurrentRenderer->GetActors();
                vtkCollectionSimpleIterator ait;
                for (ac->InitTraversal(ait); (anActor = ac->GetNextActor(ait)); )
                {
                    for (anActor->InitPathTraversal(); (path=anActor->GetNextPath()); )
                    {
                        aPart=static_cast<vtkActor *>(path->GetLastNode()->GetViewProp());
                        aPart->GetProperty()->SetRepresentationToSurface();
                    }
                }
            }
            else
            {
                vtkWarningMacro(<<"no current renderer on the interactor style.");
            }
            rwi->Render();
        }
        break;

    case '3' :
        if (rwi->GetRenderWindow()->GetStereoRender()) 
        {
            rwi->GetRenderWindow()->StereoRenderOff();
        }
        else 
        {
            rwi->GetRenderWindow()->StereoRenderOn();
        }
        rwi->Render();
        break;

    case 'p' :
    case 'P' :
        if(this->CurrentRenderer!=0)
        {
            if (this->State == VTKIS_NONE)
            {
                vtkAssemblyPath *path = NULL;
                int *eventPos = rwi->GetEventPosition();
                this->FindInteractiveRenderer(eventPos[0], eventPos[1]);
                rwi->StartPickCallback();
                vtkAbstractPropPicker *picker =
                    vtkAbstractPropPicker::SafeDownCast(rwi->GetPicker());
                if ( picker != NULL )
                {
                    picker->Pick(eventPos[0], eventPos[1],
                        0.0, this->CurrentRenderer);
                    path = picker->GetPath();
                }
                if ( path == NULL )
                {
                    this->HighlightProp(NULL);
                    this->PropPicked = 0;
                }
                else
                {
                    this->HighlightProp(path->GetFirstNode()->GetViewProp());
                    this->PropPicked = 1;
                }
                rwi->EndPickCallback();
            }
        }
        else
        {
            vtkWarningMacro(<<"no current renderer on the interactor style.");
        }
        break;
    }
}

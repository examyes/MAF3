/*==============================================================================

Program:   Multimod Application framework RELOADED
Module:    $RCSfile: vtkMAFContourVolumeMapper.cxx,v $
Language:  C++
Date:      $Date: 2007/02/09 15:51:45 $
Version:   $Revision: 1.17 $
Authors:   Alexander Savenko, Nigel McFarlane, Paolo Quadrani

================================================================================
Copyright (c) 2002/2006 University of Bedfordshire, UK (www.beds.ac.uk) 
All rights reserved.
===============================================================================*/


#ifndef VTK_IMPLEMENT_MESA_CXX
# include <vtkOpenGL.h>
#endif

#include "vtkMAFContourVolumeMapper.h"
#include "vtkMAFPolyline2D.h"
#include "vtkMAFListOfPolyline2D.h"

#include <assert.h>
#include <vector>

#include <vtkObjectFactory.h>
#include <vtkMatrix4x4.h>
#include <vtkMath.h>
#include <vtkVolume.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkCamera.h>
#include <vtkTransform.h>
#include <vtkPlaneCollection.h>
#include <vtkPlane.h>
#include <vtkDataArray.h>
#include <vtkCellArray.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkImageData.h>
#include <vtkRectilinearGrid.h>
#include <vtkTimerLog.h>
#include <vtkVolumeProperty.h>
#include <vtkMarchingCubesCases.h>
#include <vtkMarchingSquaresCases.h>
#include <vtkInformation.h>
#include <vtkAlgorithmOutput.h>
#include <vtkAlgorithm.h>
#include <vtkDataObject.h>


#include <algorithm>
#include <cmath>
#include <fstream>

static const vtkMarchingCubesTriangleCases* marchingCubesCases = vtkMarchingCubesTriangleCases::GetCases();

using namespace vtkMAFContourVolumeMapperNamespace;

#ifndef VTK_IMPLEMENT_MESA_CXX
vtkStandardNewMacro(vtkMAFContourVolumeMapper);
#endif


//------------------------------------------------------------------------------
// Constructor
vtkMAFContourVolumeMapper::vtkMAFContourVolumeMapper()
//------------------------------------------------------------------------------
{
    Opacity = 1.0;

    this->AutoLODRender = true;
    this->AutoLODCreate = true;
    this->EnableContourAnalysis = false;

    this->BlockMinMax = NULL;

    this->VoxelCoordinates[0] = this->VoxelCoordinates[1] = this->VoxelCoordinates[2] = NULL;

    this->TransformMatrix = vtkMatrix4x4::New();
    this->VolumeMatrix    = vtkMatrix4x4::New();

    for (int lod = 0 ;  lod < NumberOfLods ;  lod++){
        // set initial value of no. of triangles to undefined
        this->NumberOfTriangles[lod] = -1 ;

        // initialize times to draw to undefined
        this->TimeToDrawDC[lod] = -1.0 ;
        this->TimeToDrawRMC[lod] = -1.0 ;

        // initialize caches to NULL
        this->TriangleCache[lod] = NULL;
        this->TriangleCacheSize[lod] = 0;

        // sorting triangles
        this->ordered_vertices[lod] = NULL ;
    }

    this->TimePerTriangle = -1.0 ;

    this->SetNumberOfInputPorts(1);
    this->SetNumberOfOutputPorts(1);
}


//------------------------------------------------------------------------------
// Destructor
vtkMAFContourVolumeMapper::~vtkMAFContourVolumeMapper()
//------------------------------------------------------------------------------
{
    // delete textures if any
    ReleaseData();

    this->TransformMatrix->Delete();
    this->VolumeMatrix->Delete();
}


//----------------------------------------------------------------------------
int vtkMAFContourVolumeMapper::FillInputPortInformation( int /*port*/, vtkInformation* info) {
    // All input ports consume polygonal data. 
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");

    return 1;
}

//----------------------------------------------------------------------------
int vtkMAFContourVolumeMapper::FillOutputPortInformation( int /*port*/, vtkInformation* info) {
    // All output ports produce polygonal data.
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkDataSet");

    return 1;
}

//------------------------------------------------------------------------------
// Delete allocated data
// Called by destructor, and where input data has changed - SetInput() and
// PrepareAccelerationDataTemplate().
void vtkMAFContourVolumeMapper::ReleaseData()
//------------------------------------------------------------------------------
{
    delete [] this->BlockMinMax;
    this->BlockMinMax = NULL;

    delete [] this->VoxelCoordinates[0];
    delete [] this->VoxelCoordinates[1];
    delete [] this->VoxelCoordinates[2];
    this->VoxelCoordinates[0] = this->VoxelCoordinates[1] = this->VoxelCoordinates[2] = NULL;

    for (int lod = 0 ;  lod < NumberOfLods ;  lod++){
        // re-initialize no. of triangles and times to draw
        this->NumberOfTriangles[lod] = -1 ;
        this->TimeToDrawDC[lod] = -1.0 ;
        this->TimeToDrawRMC[lod] = -1.0 ;

        // delete triangle caches
        delete [] this->TriangleCache[lod];
        this->TriangleCache[lod] = NULL;
        this->TriangleCacheSize[lod] = 0;

        // delete vertex order array
        delete[] ordered_vertices[lod] ;
        ordered_vertices[lod] = NULL ;
    }


    this->TimePerTriangle = -1.0 ;
}




//------------------------------------------------------------------------------
// Free caches and set stats to undefined, eg after contour value changes
void vtkMAFContourVolumeMapper::ClearCachesAndStats()
//------------------------------------------------------------------------------
{
    for (int lod = 0 ;  lod < NumberOfLods ;  lod++) {
        // delete triangle caches
        delete [] this->TriangleCache[lod];
        this->TriangleCache[lod] = NULL;
        this->TriangleCacheSize[lod] = 0;

        // delete vertex order array
        delete[] ordered_vertices[lod] ;
        ordered_vertices[lod] = NULL ;

        // set stats to undefined
        this->NumberOfTriangles[lod] = -1 ;
        this->TimeToDrawDC[lod] = -1.0 ;
        this->TimeToDrawRMC[lod] = -1.0 ;
    }

    this->CacheCreated = false ;
}

//------------------------------------------------------------------------------
// This is the first function to be called before Render()
// It is used by mafPipeIsosurface::Create() to set the intial contour to an acceptable value.
// Calls EstimateRelevantVolumeTemplate() with correct scalar datatype
// Returns the fraction of blocks which contain the contour.
float vtkMAFContourVolumeMapper::EstimateRelevantVolume(const double contourValue)
//------------------------------------------------------------------------------
{
    switch (this->GetDataType()) {
    case VTK_CHAR:
        return EstimateRelevantVolumeTemplate((char)contourValue);
    case VTK_UNSIGNED_CHAR:
        return EstimateRelevantVolumeTemplate((unsigned char)contourValue);
    case VTK_SHORT:
        return EstimateRelevantVolumeTemplate((short)contourValue);
    case VTK_UNSIGNED_SHORT:
        return EstimateRelevantVolumeTemplate((unsigned short)contourValue);
    case VTK_FLOAT:
        return EstimateRelevantVolumeTemplate((float)contourValue);
    default:
        return 0.f;
    }
    return 0.f;
}

//------------------------------------------------------------------------------
// Template function corresponding to EstimateRelevantVolume()
// Calls PrepareAccelerationDataTemplate() to set up the blocks,
// and returns the fraction of blocks which contain the contour.
template <typename T> float vtkMAFContourVolumeMapper::EstimateRelevantVolumeTemplate(const T ContourValue)
//------------------------------------------------------------------------------
{
    vtkAlgorithm *producer = GetInputConnection(0, 0)->GetProducer();
    if(!producer) return 0.;
    vtkDataObject *dataObject = producer->GetOutputDataObject(0);
    if(!dataObject) return 0.;
    vtkDataSet* dataset = vtkDataSet::SafeDownCast(dataObject);

    if (dataset == NULL || dataset->GetPointData() == NULL || dataset->GetPointData()->GetScalars() == NULL)
        return 0.f;

    const T *dataPointer = (const T *)dataset->GetPointData()->GetScalars()->GetVoidPointer(0);
    PrepareAccelerationDataTemplate((const T*)dataPointer);

    // go through the blocks now
    int relevant = 0;
    const T (*BlockMinMax)[2] = (T (*)[2])this->BlockMinMax;
    for (int bzi = 0; bzi < this->NumBlocks[2]; bzi++) {
        const int zblockSize = (((bzi + 1) < this->NumBlocks[2]) ? VoxelBlockSize : (this->DataDimensions[2] - 1 - (bzi << VoxelBlockSizeLog)));

        for (int byi = 0; byi < this->NumBlocks[1]; byi++) {
            const int yblockSize = (((byi + 1) < this->NumBlocks[1]) ? VoxelBlockSize : (this->DataDimensions[1] - 1 - (byi << VoxelBlockSizeLog)));

            for (int bxi = 0; bxi < this->NumBlocks[0]; bxi++) {
                const int block = bxi + this->NumBlocks[0] * (byi + bzi * this->NumBlocks[1]);
                const int xblockSize = (((bxi + 1) < this->NumBlocks[0]) ? VoxelBlockSize : (this->DataDimensions[0] - 1 - (bxi << VoxelBlockSizeLog)));

                if (ContourValue >= BlockMinMax[block][0] && ContourValue <= BlockMinMax[block][1])
                    relevant++;
            }
        }
    }

    return (float)relevant / (this->NumBlocks[0] * this->NumBlocks[1] * this->NumBlocks[2] - 1);
}

//------------------------------------------------------------------------------
// Render the iso-surface
// If data has been cached for this contour value
//        Calls DrawCache()
// Else
//        Calls PrepareAccelerationDataTemplate()
//              RenderMCubes()
void vtkMAFContourVolumeMapper::Render(vtkRenderer *renderer, vtkVolume *volume)
//------------------------------------------------------------------------------
{
    vtkAlgorithm *producer = GetInputConnection(0, 0)->GetProducer();
    if(!producer) return;
    vtkDataObject *dataObject = producer->GetOutputDataObject(0);
    if(!dataObject) return;
    vtkDataSet* dataset = vtkDataSet::SafeDownCast(dataObject);

    if (dataset == NULL || dataset->GetPointData() == NULL || dataset->GetPointData()->GetScalars() == NULL) {
        vtkErrorMacro(<< "No data for rendering");
        return;
    }

    // clear the caches and statistics if the contour value has changed
    if (this->ContourValue != this->PrevContourValue)
        ClearCachesAndStats() ;

    // find the highest lod which DrawCache() can draw
    int lod = BestLODForDrawCache(renderer) ;

    // if this lod is cached, draw it, else need to go to RenderMCubes() to get it cached
    if (this->CacheCreated && (this->TriangleCache[lod] != NULL)) {
        this->DrawCache(renderer, volume, lod);
        return;
    }

    const void *dataPointer = dataset->GetPointData()->GetScalars()->GetVoidPointer(0);

    switch (this->GetDataType()) {
    case VTK_CHAR:
        if (PrepareAccelerationDataTemplate((const char*)dataPointer))
            this->RenderMCubes(renderer, volume, (const char*)dataPointer);
        break;
    case VTK_UNSIGNED_CHAR:
        if (PrepareAccelerationDataTemplate((const unsigned char*)dataPointer)) {
            this->RenderMCubes(renderer, volume, (const unsigned char*)dataPointer);
        }
        break;
    case VTK_SHORT:
        if (PrepareAccelerationDataTemplate((const short*)dataPointer))
            this->RenderMCubes(renderer, volume, (const short*)dataPointer);
        break;
    case VTK_UNSIGNED_SHORT:
        if (PrepareAccelerationDataTemplate((const unsigned short*)dataPointer))
            this->RenderMCubes(renderer, volume, (const unsigned short*)dataPointer);
        break;
    case VTK_FLOAT:
        if (PrepareAccelerationDataTemplate((const float*)dataPointer))
            this->RenderMCubes(renderer, volume, (const float*)dataPointer);
        break;
    default:
        vtkErrorMacro(<< "Only 8/16 bit integers and 32 bit floats are supported.");
    }
}

//------------------------------------------------------------------------------
// Set up the acceleration structures if data has changed
// Gets data extents
// Calculates coords of every voxel, and offsets between neighboring voxels
// Divides volume into 8x8x8 blocks and calculates the min and max of each block.
template <typename T> bool vtkMAFContourVolumeMapper::PrepareAccelerationDataTemplate(const T *dataPointer)
//------------------------------------------------------------------------------
{
    int lod ;
    vtkAlgorithm *producer = GetInputConnection(0, 0)->GetProducer();
    if(!producer) return false;
    vtkDataObject *dataObject = producer->GetOutputDataObject(0);
    if(!dataObject) return false;
    vtkDataSet* dataset = vtkDataSet::SafeDownCast(dataObject);

    // Is data the same?
    if (dataset != NULL && dataset->GetMTime() > this->BuildTime)
        this->ReleaseData();
    else if (this->BlockMinMax != NULL)
        return true; // nothing to do

    // check the data
    if (dataset && dataset->GetDataReleased())
        dataset->Update(); // ensure that the data is loaded
    if (!this->IsDataValid(true))
        return false;

    // cast the input to image or rectilinear grid (one will be valid, one should be NULL)
    vtkImageData       *imageData = vtkImageData::SafeDownCast(dataset);
    vtkRectilinearGrid *gridData  = vtkRectilinearGrid::SafeDownCast(dataset);

    // find data extent (dims, origin, spacing)
    // For each axis, calculates coords of the voxels, with padding of one VoxelBlockSize at end of array.
    // Each block is a 8x8x8 cube with VoxelBlockSizeLog = 3, VoxelBlockSize = 8, VoxelsInBlock = 8^3
    if (imageData) {
        imageData->GetDimensions(this->DataDimensions);
        imageData->GetOrigin(this->DataOrigin);
        imageData->GetSpacing(this->DataSpacing);
        for (int axis = 0; axis < 3; axis++) {
            // delete and reallocate array of coordinates
            delete [] this->VoxelCoordinates[axis];
            this->VoxelCoordinates[axis] = new float [this->DataDimensions[axis] + VoxelBlockSize + 1];    // NMcF: why do you need the extra +1 ?

            // set voxel coords, with padding of one block size
            float f = this->DataOrigin[axis];
            int i;
            for (i = 0; i < this->DataDimensions[axis]; i++, f += this->DataSpacing[axis])
                this->VoxelCoordinates[axis][i] = f;
            for (i = 0; i < VoxelBlockSize; i++)
                this->VoxelCoordinates[axis][this->DataDimensions[axis] + i] = f;
        }
    } else if (gridData) {
        // same stuff, calculated differently for rectilinear grid
        gridData->GetDimensions(this->DataDimensions);
        this->DataOrigin[0] = gridData->GetXCoordinates()->GetTuple(0)[0];
        this->DataOrigin[1] = gridData->GetYCoordinates()->GetTuple(0)[0];
        this->DataOrigin[2] = gridData->GetZCoordinates()->GetTuple(0)[0];
        this->DataSpacing[0] = gridData->GetXCoordinates()->GetTuple(1)[0] - this->DataOrigin[0];
        this->DataSpacing[1] = gridData->GetYCoordinates()->GetTuple(1)[0] - this->DataOrigin[1];
        this->DataSpacing[2] = gridData->GetZCoordinates()->GetTuple(1)[0] - this->DataOrigin[2];

        for (int axis = 0; axis < 3; axis++) {
            // delete and reallocate array of coordinates
            delete [] this->VoxelCoordinates[axis];
            this->VoxelCoordinates[axis] = new float [this->DataDimensions[axis] + VoxelBlockSize + 1];

            // set voxel coords, with padding of one block size
            vtkDataArray *coordinates = (axis == 2) ? gridData->GetZCoordinates() : (axis == 1 ? gridData->GetYCoordinates() : gridData->GetXCoordinates());
            int i;
            for (i = 0; i < this->DataDimensions[axis]; i++)
                this->VoxelCoordinates[axis][i] = *(coordinates->GetTuple(i));
            for (i = 0; i < VoxelBlockSize; i++)
                this->VoxelCoordinates[axis][this->DataDimensions[axis] + i] = this->VoxelCoordinates[axis][this->DataDimensions[axis] - 1];
        }
    } else {
        // data not image or rect. grid
        vtkErrorMacro(<< "ContourVolumeMapper: vtk data is not image or rect. grid");
    }

    // calculate offsets between neighboring voxels for all levels of detail
    // (used later in RenderMCubes())
    //               ___
    //     ^ y     /|3 2|
    //     |   x  / |0 1|
    //     /-->  / / ---
    //    /     /___ / /
    //   / z    |7 6| /
    //          |4 5|/
    //           ---
    for (lod = 0 ;  lod < NumberOfLods ;  lod++) {
        CalculateVoxelVertIndicesOffsets(lod, this->VoxelVertIndicesOffsets[lod]) ; // offsets to index in data array
    }

    // calculate no. of blocks in each direction
    // MMcF: is the +1 necessary if the dimension is an exact power of 2 ?
    for (int xyz = 0; xyz < 3; xyz++)
        this->NumBlocks[xyz] = ((this->DataDimensions[xyz] - 1) >> VoxelBlockSizeLog) + 1;

    // delete and reallocate array of block mins and maxes
    // declare a local BlockMinMax which is this->BlockMinMax cast to 2D array of correct type
    delete [] this->BlockMinMax;
    this->BlockMinMax = new T [2 * this->NumBlocks[0] * this->NumBlocks[1] * this->NumBlocks[2]];
    T (*BlockMinMax)[2] = (T (*)[2])this->BlockMinMax;    // create local variable which is 


    // set the mins and maxes to extreme values of the appropriate data type
    if (this->GetDataType() == VTK_UNSIGNED_CHAR || this->GetDataType() == VTK_UNSIGNED_SHORT) { // unsigned  
        for (int ii = 0, ilen = this->NumBlocks[0] * this->NumBlocks[1] * this->NumBlocks[2]; ii < ilen; ii++)
            BlockMinMax[ii][0] = (T)VTK_UNSIGNED_SHORT_MAX, BlockMinMax[ii][1] = 0;
    } else if (this->GetDataType() != VTK_FLOAT) { // signed
        for (int ii = 0, ilen = this->NumBlocks[0] * this->NumBlocks[1] * this->NumBlocks[2]; ii < ilen; ii++)
            BlockMinMax[ii][0] = (T)VTK_SHORT_MAX, BlockMinMax[ii][1] = (T)VTK_SHORT_MIN;
    } else { 
        float (*BlockMinMax)[2] = (float (*)[2])this->BlockMinMax;
        for (int ii = 0, ilen = this->NumBlocks[0] * this->NumBlocks[1] * this->NumBlocks[2]; ii < ilen; ii++)
            BlockMinMax[ii][0] = VTK_FLOAT_MAX, BlockMinMax[ii][1] = VTK_FLOAT_MIN;
    }



    // loop over all voxels, calculating the block index of the voxels.
    // voxels on the boundaries are counted as being in both blocks.
    for (int zi = 0, vi = 0; zi < this->DataDimensions[2]; zi++) {
        const int zblockI[2] = {(zi >> VoxelBlockSizeLog) * this->NumBlocks[0] * this->NumBlocks[1],
            zi ? ((zi - 1) >> VoxelBlockSizeLog) * this->NumBlocks[0] * this->NumBlocks[1] : 0};
        const int zblocks = (zblockI[0] == zblockI[1]) ? 1 : 2;

        for (int yi = 0; yi < this->DataDimensions[1]; yi++) {
            const int yblockI[2] = {(yi >> VoxelBlockSizeLog) * this->NumBlocks[0],
                yi ? ((yi - 1) >> VoxelBlockSizeLog) * this->NumBlocks[0] : 0};
            const int yblocks = (yblockI[0] == yblockI[1]) ? 1 : 2;

            for (int xi = 0; xi < this->DataDimensions[0]; xi++, vi++) {
                const int xblockI[2] = {xi >> VoxelBlockSizeLog, 
                    xi ? ((xi - 1) >> VoxelBlockSizeLog) : 0};
                const int xblocks = (xblockI[0] == xblockI[1]) ? 1 : 2;

                const T val = dataPointer[vi];    // pointer to scalar values was passed as an argument to this function

                // update the min/max for each block which the voxel is in.
                for (int zz = 0; zz < zblocks; zz++) {
                    for (int yy = 0; yy < yblocks; yy++) {
                        for (int xx = 0; xx < xblocks; xx++) {
                            T (&minMax)[2] = BlockMinMax[xblockI[xx] + yblockI[yy] + zblockI[zz]];    // minMax is a just a reference for BlockMinMax[i]
                            if (minMax[0] > val) // min
                                minMax[0] = val;
                            if (minMax[1] < val) // max
                                minMax[1] = val;
                        }
                    }
                }

            } //for (xi)
        } //for (yi)
    } //for (zi)


    // set contour value and cache flag to first time values
    // NMcF: shouldn't we free the cache memory as well ?
    this->PrevContourValue = VTK_FLOAT_MAX;
    this->CacheCreated = false;

    this->BuildTime.Modified();
    return true;
}

//------------------------------------------------------------------------------
void vtkMAFContourVolumeMapper::EnableClipPlanes(bool enable)
//------------------------------------------------------------------------------
{
    if (enable) {
        if (this->ClippingPlanes) {
            int numClipPlanes = this->ClippingPlanes->GetNumberOfItems();
            if (numClipPlanes > 6)
                vtkErrorMacro(<< "OpenGL guarantees only 6 additional clipping planes");

            for (int i = 0; i < this->ClippingPlanes->GetNumberOfItems(); i++) {
                glEnable((GLenum)(GL_CLIP_PLANE0+i));

                vtkPlane *plane = (vtkPlane *)this->ClippingPlanes->GetItemAsObject(i);

                double planeEquation[4];
                planeEquation[0] = plane->GetNormal()[0]; 
                planeEquation[1] = plane->GetNormal()[1]; 
                planeEquation[2] = plane->GetNormal()[2];
                planeEquation[3] = -(planeEquation[0] * plane->GetOrigin()[0] + planeEquation[1] * plane->GetOrigin()[1]+
                    planeEquation[2] * plane->GetOrigin()[2]);
                glClipPlane((GLenum)(GL_CLIP_PLANE0+i),planeEquation);
            }
        }
    } else {
        if (this->ClippingPlanes) {
            for (int i = 0; i < this->ClippingPlanes->GetNumberOfItems(); i++)
                glDisable((GLenum)(GL_CLIP_PLANE0+i));
        }
    }
}

//------------------------------------------------------------------------------
void vtkMAFContourVolumeMapper::Update()
//------------------------------------------------------------------------------
{
    vtkAlgorithm *producer = GetInputConnection(0, 0)->GetProducer();
    if(!producer) return;
    vtkDataObject *dataObject = producer->GetOutputDataObject(0);
    if(!dataObject) return;
    vtkDataSet* dataset = vtkDataSet::SafeDownCast(dataObject);
    
    if (vtkImageData::SafeDownCast(dataset) != NULL || 
        vtkRectilinearGrid::SafeDownCast(dataset) != NULL) {
            dataset->UpdateInformation();
            dataset->SetUpdateExtentToWholeExtent();
            dataset->Update();
    }
}

//------------------------------------------------------------------------------
bool vtkMAFContourVolumeMapper::IsDataValid(bool warnings)
//------------------------------------------------------------------------------
{
    vtkAlgorithm *producer = GetInputConnection(0, 0)->GetProducer();
    if(!producer) return false;
    vtkDataObject *dataObject = producer->GetOutputDataObject(0);
    if(!dataObject) return false;
    vtkDataSet* inputData = vtkDataSet::SafeDownCast(dataObject);
    
    vtkImageData       *imageData = vtkImageData::SafeDownCast(inputData);
    vtkRectilinearGrid *gridData  = vtkRectilinearGrid::SafeDownCast(inputData);

    // check the data
    if (inputData == NULL) {
        if (warnings)
            vtkErrorMacro(<< "Contour mapper: No data to render.");
        return false;
    }
    if (imageData == NULL && gridData == NULL) {
        if (warnings)
            vtkErrorMacro(<< "Contour mapper: this data format is not supported");
        return false;
    }

    int dataType = this->GetDataType();
    if (dataType != VTK_SHORT && dataType != VTK_UNSIGNED_SHORT &&
        dataType != VTK_CHAR && dataType != VTK_UNSIGNED_CHAR && dataType != VTK_FLOAT) {
            if (warnings)
                vtkErrorMacro(<< "Only 8/16 bit integers and 32 bit floats are supported.");
            return false;
    }

    // check image data
    if (imageData) {
        int extent[6];
        imageData->GetExtent(extent);
        if (extent[4] >= extent[5]) {
            if (warnings)
                vtkErrorMacro(<< "Contour mapper: 2D datasets are not supported");
            return false;
        }

        if (imageData->GetNumberOfScalarComponents() > 1) {
            if (warnings)
                vtkErrorMacro(<< "Contour mapper: only monochrome images are currently supported.");
            return false;
        }

        return true;
    }


    if (gridData) {
        if (gridData->GetPointData() == NULL || gridData->GetPointData()->GetScalars() == NULL ||
            gridData->GetXCoordinates()->GetNumberOfTuples() < 2 ||
            gridData->GetYCoordinates()->GetNumberOfTuples() < 2 ||
            gridData->GetZCoordinates()->GetNumberOfTuples() < 2) {
                if (warnings)
                    vtkErrorMacro(<< "Contour mapper: the dataset is empty.");
                return false;
        }
    }

    return true;
}

//------------------------------------------------------------------------------
// Return data type of input scalars
int vtkMAFContourVolumeMapper::GetDataType()
//------------------------------------------------------------------------------
{
    
    vtkAlgorithm *producer = GetInputConnection(0, 0)->GetProducer();
    if(!producer) return VTK_VOID;
    vtkDataObject *dataObject = producer->GetOutputDataObject(0);
    if(!dataObject) return VTK_VOID;
    vtkDataSet* dataset = vtkDataSet::SafeDownCast(dataObject);

    if(dataset) {
        int dataType = dataset->GetPointData()->GetScalars()->GetDataType();
        return dataType;
    }
    
    
    
    return VTK_VOID;
}

//------------------------------------------------------------------------------
// Return iso-surface as polydata
// Similar to Render() but calls CreateMCubes() instead of RenderMCubes()
// Default args are 0 and NULL.
// Allocates polydata if input polydata is NULL
vtkPolyData *vtkMAFContourVolumeMapper::GetOutput(int level, vtkPolyData *data)
//------------------------------------------------------------------------------
{
    vtkAlgorithm *producer = GetInputConnection(0, 0)->GetProducer();
    if(!producer) return NULL;
    vtkDataObject *dataObject = producer->GetOutputDataObject(0);
    if(!dataObject) return NULL;
    vtkDataSet* dataset = vtkDataSet::SafeDownCast(dataObject);

    // check that level is in correct range
    if (level < 0 || level >= NumberOfLods)
        return NULL;

    // check that volume data is valid
    if (dataset == NULL || dataset->GetPointData() == NULL || dataset->GetPointData()->GetScalars() == NULL) {
        vtkErrorMacro(<< "No data");
        return NULL;
    }

    // use input polydata or allocate if none
    vtkPolyData *polydata = (data == NULL) ? vtkPolyData::New() : data;

    const void *dataPointer = dataset->GetPointData()->GetScalars()->GetVoidPointer(0);

    switch (this->GetDataType()) {
    case VTK_CHAR:
        if (!PrepareAccelerationDataTemplate((const char*)dataPointer))
            return NULL;
        this->CreateMCubes(level, polydata, (const char*)dataPointer);
        break;
    case VTK_UNSIGNED_CHAR:
        if (!PrepareAccelerationDataTemplate((const unsigned char*)dataPointer))
            return NULL;
        this->CreateMCubes(level, polydata, (const unsigned char*)dataPointer);
        break;
    case VTK_SHORT:
        if (!PrepareAccelerationDataTemplate((const short*)dataPointer))
            return NULL;
        this->CreateMCubes(level, polydata, (const short*)dataPointer);
        break;
    case VTK_UNSIGNED_SHORT:
        if (!PrepareAccelerationDataTemplate((const unsigned short*)dataPointer))
            return NULL;
        this->CreateMCubes(level, polydata, (const unsigned short*)dataPointer);
        break;
    case VTK_FLOAT:
        if (!PrepareAccelerationDataTemplate((const float*)dataPointer))
            return NULL;
        this->CreateMCubes(level, polydata, (const float*)dataPointer);
        break;
    default:
        vtkErrorMacro(<< "Only 8/16 bit integers and 32 bit floats are supported.");
    }

    // create polydata
    return polydata;
}

//------------------------------------------------------------------------------
// Initialize OpenGL rendering, called by DrawCache() and RenderMCubes()
void vtkMAFContourVolumeMapper::InitializeRender(bool setup, vtkRenderer *renderer, vtkVolume *volume)
//------------------------------------------------------------------------------
{
    glGetError(); // reset error flag
    if (setup) {
        glPushAttrib(GL_ALL_ATTRIB_BITS);

        EnableClipPlanes(true);

        //Matteo
        glEnable(GL_BLEND);
        //End

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);

        glShadeModel(GL_SMOOTH);
        glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);

        // disable any anti-aliasing
        bool multisamplingExt = (strstr((const char *)glGetString(GL_EXTENSIONS), "GL_EXT_multisample") != NULL ||
            strstr((const char *)glGetString(GL_EXTENSIONS), "GL_SGIS_multisample") != NULL);
        if (multisamplingExt)
            glDisable(0x809D);
        glHint(GL_POLYGON_SMOOTH_HINT, GL_FASTEST);
        glDisable(GL_POLYGON_SMOOTH);

        glPixelTransferf(GL_RED_SCALE, 1.f);
        glPixelTransferf(GL_GREEN_SCALE, 1.f);
        glPixelTransferf(GL_BLUE_SCALE, 1.f);

        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // ----------- initialize state variables 

        // viewport
        float viewport[4];
        glGetFloatv(GL_VIEWPORT, viewport);
        this->ViewportDimensions[0] = viewport[2];
        this->ViewportDimensions[1] = viewport[3];

        // transformation
        this->TransformMatrix->DeepCopy(renderer->GetActiveCamera()->GetCompositeProjectionTransformMatrix((double)viewport[2] / viewport[3], 0, 1));
        volume->GetMatrix(this->VolumeMatrix);
        vtkMatrix4x4::Multiply4x4(this->TransformMatrix, this->VolumeMatrix, this->TransformMatrix);
        this->VolumeMatrix->Transpose();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glMultMatrixd(this->VolumeMatrix->Element[0]);
        volume->GetMatrix(this->VolumeMatrix); // get the matrix again

        // set up materials
        glDisable(GL_COLOR_MATERIAL);
        glColor3f(1.f, 1.f, 1.f);
        float ambientColor  = volume->GetProperty()->GetAmbient();
        float diffuseColor  = volume->GetProperty()->GetDiffuse();
        float specularColor = volume->GetProperty()->GetSpecular();
        float ambientColor3[4] = { ambientColor, ambientColor, ambientColor, Opacity};
        float diffuseColor3[4] = { diffuseColor, diffuseColor, diffuseColor, Opacity};
        float specularColor3[4] = { specularColor, specularColor, specularColor, Opacity};
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,  ambientColor3);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,  diffuseColor3);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor3);
    } else { // restore the settings
        glPopMatrix();

        glPopAttrib();

        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }
    assert(glGetError() == GL_NO_ERROR);
}

//------------------------------------------------------------------------------
// OpenGL code which initializes OpenGL and render the cached triangles.
// Draws either normal or LOD cache
void vtkMAFContourVolumeMapper::DrawCache(vtkRenderer *renderer, vtkVolume *volume, int lod)
//------------------------------------------------------------------------------
{
    // use cache if contour value not changed and cache exists
    const bool useCache = (this->PrevContourValue == this->ContourValue)
        && this->CacheCreated
        && (this->TriangleCache[lod] != NULL)
        && (this->NumberOfTriangles[lod] >= 0) ;
    assert(useCache);

    this->Timer->StartTimer();

    int nvert = 3*this->NumberOfTriangles[lod] ;
    InitializeRender(true, renderer, volume);


    bool sortall = false ;

    if (this->Opacity < 1.0) {
        // transparency enabled so sort triangles and render

        sortall = (renderer->GetAllocatedRenderTime() > (float)(SortFraction)*this->TimeToDrawDC[lod]) ;
        SortTriangles(lod, sortall) ;

        if (this->NumberOfTriangles[lod] > 0) {
            glDisable(GL_DEPTH_TEST) ;
            glInterleavedArrays(GL_N3F_V3F, 0, this->TriangleCache[lod]);
            if (this->NumberOfTriangles[lod] > 0)
                glDrawElements(GL_TRIANGLES, nvert, GL_UNSIGNED_INT, ordered_vertices[lod]);
        }
    } else {
        // render the triangles
        glEnable(GL_DEPTH_TEST) ;
        if (this->NumberOfTriangles[lod] > 0) {
            glInterleavedArrays(GL_N3F_V3F, 0, this->TriangleCache[lod]);
            if (this->NumberOfTriangles[lod] > 0) {
                glDrawArrays(GL_TRIANGLES, 0, nvert);
            }
        }
    }

    InitializeRender(false);

    this->Timer->StopTimer();

    // note time taken to draw triangles
    this->TimeToDraw = (float)this->Timer->GetElapsedTime();

    if ((this->Opacity < 1.0) && sortall) {
        // transparency on: sorting all triangles at once is not supposed to be typical, 
        // and should only happen when time is not critical,
        // so we only save a fraction of the time taken.
        this->TimeToDrawDC[lod] = this->TimeToDraw / (float)SortFraction ;
    } else {
        this->TimeToDrawDC[lod] = this->TimeToDraw ;
    }

} // DrawCache()



//------------------------------------------------------------------------------
// Marching cubes algorithm
// Constructs triangles and renders them.
// Predicts no. of triangles and time to draw before rendering, and chooses appropriate level of detail.
// Triangles are cached if possible.
template<typename T> void vtkMAFContourVolumeMapper::RenderMCubes(vtkRenderer *renderer, vtkVolume *volume, const T *dataPointer)
//------------------------------------------------------------------------------
{
    int lod ;
    int numTrianglesRunningTotal[4] = {0,0,0,0} ;       // keeps running total of triangles

    // Get max level of detail
    int LODLevel = BestLODForRenderMCubes(renderer) ;

    // Decide whether caching can be enabled
    // Caching enabled if  no cache already, 
    //                     no. of triangles is known exactly (ie it's been rendered once already),
    //                     and not too many triangles.
    bool createCache = !this->CacheCreated && (this->NumberOfTriangles[LODLevel] >= 0) && (this->NumberOfTriangles[LODLevel] <= MaxTrianglesNotOptimized) ;


    // Decide on range of lods to use
    // if no autolod, use only 0
    // if caching, cache lodlevel... nlods-1 (but only render top level)
    // if no caching, use only lodlevel
    int firstLOD, lastLOD ;
    firstLOD = LODLevel ;
    if (!this->AutoLODRender) {
        lastLOD = 0 ;
    } else if (createCache) {
        lastLOD = NumberOfLods - 1 ;
    } else {
        lastLOD = LODLevel ;
    }

    if (createCache) {
        // allocate caches for each lod in range
        // since we are caching, NumberOfTriangles[LODLevel] must be valid, so we allocate all caches to this value
        for (lod = firstLOD ; lod <= lastLOD ;  lod++) {
            if (this->TriangleCacheSize[lod] < this->NumberOfTriangles[LODLevel] || this->TriangleCache[lod] == NULL) {
                // if cache not big enough, or not defined, reallocate cache
                // 3 vertices per triangle, 3 normals + 3 coords per vertex = 18
                delete [] this->TriangleCache[lod];
                this->TriangleCache[lod] = new float [18 * this->NumberOfTriangles[LODLevel]];

                // if allocation failed, set createCache back to false
                if (this->TriangleCache[lod] == NULL) {
                    createCache = false;
                    this->TriangleCacheSize[lod] = 0;
                    vtkErrorMacro(<< "Contour mapper: not enough memory");
                    break;
                }

                // cache size in no. of triangles
                this->TriangleCacheSize[lod] = this->NumberOfTriangles[lod];
            }
        }
    }

    if (!createCache) {
        // no caching (or caching failed)
        // clear all caches
        for (lod = 0 ;  lod < NumberOfLods ;  lod++) {
            delete [] this->TriangleCache[lod];
            this->TriangleCache[lod] = NULL;
            this->TriangleCacheSize[lod] = 0;
        }

        // allocate cache LODLevel as a buffer for block triangles
        // (use LODLevel rather than zero because it simplifies things later)
        this->TriangleCacheSize[LODLevel] = 5 * VoxelsInBlock ; // 5 triangles per voxel
        this->TriangleCache[LODLevel] = new float [18 * this->TriangleCacheSize[LODLevel]];
    }

    this->CacheCreated = createCache;

    // start the timer and initialize OpenGL
    this->TimeToDrawRMC[LODLevel] = 0;
    this->Timer->StartTimer();
    InitializeRender(true, renderer, volume);

    // OpenGL: attach triangle cache LODLevel to GL array with 3 normals + 3 vertices per item
    // Only need TriangleCache[LODLevel] because if caching, only render this level; if not caching, render from block buffer, which is same
    glInterleavedArrays(GL_N3F_V3F, 0, this->TriangleCache[LODLevel]);

    // define local variables to reduce overheads and improve readability
    const T (*BlockMinMax)[2] = (T (*)[2])this->BlockMinMax;
    T ContourValue = (T)this->ContourValue;

    float *verticeArray[NumberOfLods] ;
    float *normalArray[NumberOfLods] ;
    for (lod = 0 ;  lod < NumberOfLods ;  lod++) {
        // initialize pointers to NULL
        normalArray[lod]  = NULL ;
        verticeArray[lod] = NULL ;
    }
    for (lod = firstLOD ;  lod <= lastLOD ;  lod++) {
        if (createCache) {
            // point arrays to start of respective caches
            normalArray[lod]  = this->TriangleCache[lod] ;
            verticeArray[lod] = this->TriangleCache[lod] + 3;
        } else {
            // point all arrays to start of block buffer
            normalArray[lod]  = this->TriangleCache[LODLevel] ;
            verticeArray[lod] = this->TriangleCache[LODLevel] + 3;
        }
    }

    // process blocks and voxels
    this->SkippedVoxelBlocks = 0;
    this->VoxelsRendered = this->VoxelsSkipped = 0;

    // initialization
    this->PrevContourValue = this->ContourValue;
    for (lod = firstLOD ;  lod <= lastLOD ;  lod++)
        this->NumberOfTriangles[lod] = 0;


    // no. of voxels per slice and per row
    const int rowSize   = this->DataDimensions[0];
    const int sliceSize = this->DataDimensions[0] * this->DataDimensions[1];

    // create array of vtkMAFListOfPolyline2D pointers polylines[0..dimz-1] and set them all to zero
    vtkMAFListOfPolyline2D **polylines = new vtkMAFListOfPolyline2D*[this->DataDimensions[2]];
    memset(polylines, 0, this->DataDimensions[2]*sizeof(vtkMAFListOfPolyline2D*));

    if (EnableContourAnalysis) {
        for (int z = 0; z < this->DataDimensions[2]; z++) {
            polylines[z] = new vtkMAFListOfPolyline2D();
            this->PrepareContours(z, dataPointer + z * sliceSize, *polylines[z]);
        }
    }

    //---------------------------
    // loop over levels of detail
    //---------------------------
    for (lod = firstLOD; lod <= lastLOD; lod++) {
        // Calculate voxel increments in xy plane and in z
        // Voxel increment in xy is 1 for lod=0, 2 for lod=1, 4 for lod=2, etc
        // However, z resolution between slices may already be poor,
        // so lodz might be less than lodxy.
        int lodxy, lodz ;
        CalculateLodIncrements(lod, &lodxy, &lodz) ;

        // select cube of offsets corresponding to LOD
        const int *VoxelOffsets = this->VoxelVertIndicesOffsets[lod] ; // offsets to index in data array

        // last xyz in volume and index offsets to next row and slice
        // these are used later in the gradient calculations
        const int lastIndex[3] = {this->DataDimensions[0] - 1, this->DataDimensions[1] - 1, this->DataDimensions[2] - 1};


        //----------------------------
        // loop through all the blocks
        //----------------------------
        for (int bzi = 0; bzi < this->NumBlocks[2]; bzi++) {
            // calculate block size in z (all blocks are VoxelBlockSize except last, which is remainder - 1)
            // n.b this means that the last block will be zero in size if the remainder is 1
            const int zblockSize = (((bzi + 1) < this->NumBlocks[2]) ? VoxelBlockSize : (this->DataDimensions[2] - 1 - (bzi << VoxelBlockSizeLog)));

            for (int byi = 0; byi < this->NumBlocks[1]; byi++) {
                // calculate block size in y
                const int yblockSize = (((byi + 1) < this->NumBlocks[1]) ? VoxelBlockSize : (this->DataDimensions[1] - 1 - (byi << VoxelBlockSizeLog)));

                for (int bxi = 0; bxi < this->NumBlocks[0]; bxi++) {
                    // calculate block size in x
                    const int xblockSize = (((bxi + 1) < this->NumBlocks[0]) ? VoxelBlockSize : (this->DataDimensions[0] - 1 - (bxi << VoxelBlockSizeLog)));

                    // calculate block no.
                    const int block = bxi + this->NumBlocks[0] * (byi + bzi * this->NumBlocks[1]);

                    // skip the block if any of the sizes are zero
                    if (xblockSize == 0 || yblockSize == 0 || zblockSize == 0){
                        this->SkippedVoxelBlocks += 1.f;
                        continue;
                    }

                    // skip the block if contour value is outside range min-max
                    if ((ContourValue < BlockMinMax[block][0]) || (ContourValue > BlockMinMax[block][1])) {
                        this->SkippedVoxelBlocks += 1.f;
                        continue;
                    }


                    // set arrays for vertices and normals
                    if (!createCache) {
                        // need to reset pointers to start of block buffer
                        verticeArray[lod] = this->TriangleCache[LODLevel] + 3;
                        normalArray[lod]  = this->TriangleCache[LODLevel];
                    } else if (this->TriangleCache[lod] == NULL) {
                        continue; // no memory do not create this cache
                    }

                    float* (&vertices) = verticeArray[lod];
                    float* (&normals)  = normalArray[lod];

                    //--------------------------------
                    // Process the voxels in the block
                    //--------------------------------

                    // set up pointLocator array float* pointLocator[][][][3], ie 3 float pointers for every voxel in block.
                    // pointLocator is an array of pointers to every edge in the block, 
                    // so it will become an index of the triangle vertices.
                    // Ideally would allocate pointLocator[VBS+lodxy][VBS+lodxy][VBS+lodz][3] but array dimensions have to be const
                    const int MaxLodStep = 1 << (NumberOfLods - 1) ;  // max possible value of lodxy or lodz
                    float* pointLocator[VoxelBlockSize + MaxLodStep][VoxelBlockSize + MaxLodStep][VoxelBlockSize + MaxLodStep][3]; // three edges per voxel
                    memset(pointLocator, 0, sizeof(pointLocator));

                    // index of first voxel in block
                    const int voxelIBlock = VoxelBlockSize * (bzi * sliceSize + byi * rowSize + bxi);

                    // initialize voxel count for block
                    int numberOfTrianglesInBlock = 0;



                    //---------------------------------------------
                    // loop over voxels in block
                    //---------------------------------------------
                    for (int zi = 0; zi < zblockSize; zi += lodz) {
                        const int   z  = (bzi << VoxelBlockSizeLog) + zi;                       // voxel z index relative to volume
                        if ((z + lodz) >= this->DataDimensions[2])                              // skip if volume dimension exceeded
                            continue;
                        const float fz = this->VoxelCoordinates[2][z];                          // voxel z coord
                        const float fzSize = this->VoxelCoordinates[2][z + lodz] - fz;          // size of LOD voxel                    
                        const float fzSizeNorm = 1.f / (this->VoxelCoordinates[2][z + 1] - fz); // normalization factor : size of normal voxel                        
                        if (fzSize < 0.0001f) // arbitrary minimum size
                            continue;

                        for (int yi = 0; yi < yblockSize; yi += lodxy) {
                            const int   y  = (byi << VoxelBlockSizeLog) + yi;
                            if ((y + lodxy) >= this->DataDimensions[1])
                                continue;
                            const float fy = this->VoxelCoordinates[1][y];
                            const float fySize     = this->VoxelCoordinates[1][y + lodxy] - fy;
                            const float fySizeNorm = 1.f / (this->VoxelCoordinates[1][y + 1] - fy);

                            for (int xi = 0; xi < xblockSize; xi += lodxy) {
                                const int x = (bxi << VoxelBlockSizeLog) + xi;
                                if ((x + lodxy) >= this->DataDimensions[0])
                                    continue;
                                const float fx = this->VoxelCoordinates[0][x];
                                const float fxSize = this->VoxelCoordinates[0][x + lodxy] - fx;
                                const float fxSizeNorm = 1.f / (this->VoxelCoordinates[0][x + 1] - fx);

                                const int voxelI = voxelIBlock + zi * sliceSize + yi * rowSize + xi;    // index of current voxel
                                const T * const voxelPtr = dataPointer + voxelI;                 // pointer to current voxel
                                const float fxyzSize[3] = { fxSize, fySize, fzSize };                   // size (LOD size) of current voxel


                                // Find the marching cubes case

                                // get value of voxel and neighbors (VoxelOffsets was set earlier to normal or LOD sized cube)
                                const T voxelVals[8] = {voxelPtr[0], voxelPtr[VoxelOffsets[1]], voxelPtr[VoxelOffsets[2]], voxelPtr[VoxelOffsets[3]],
                                    voxelPtr[VoxelOffsets[4]], voxelPtr[VoxelOffsets[5]], voxelPtr[VoxelOffsets[6]], voxelPtr[VoxelOffsets[7]]};

                                // calculate case, each bit of caseIndex is set if corner > contour value
                                int caseIndex = voxelVals[0] > ContourValue; 
                                if(MAXScalar==ContourValue) {
                                    // This allows you to get contour at max of range, else can't get last slice in test volume (Matteo 27.06.06) 
                                    caseIndex = voxelVals[0] != ContourValue;
                                    caseIndex |= (voxelVals[1] != ContourValue) << 1;
                                    caseIndex |= (voxelVals[2] != ContourValue) << 2;
                                    caseIndex |= (voxelVals[3] != ContourValue) << 3;
                                    caseIndex |= (voxelVals[4] != ContourValue) << 4;
                                    caseIndex |= (voxelVals[5] != ContourValue) << 5;
                                    caseIndex |= (voxelVals[6] != ContourValue) << 6;
                                    caseIndex |= (voxelVals[7] != ContourValue) << 7;
                                } else {
                                    caseIndex |= (voxelVals[1] > ContourValue) << 1;
                                    caseIndex |= (voxelVals[2] > ContourValue) << 2;
                                    caseIndex |= (voxelVals[3] > ContourValue) << 3;
                                    caseIndex |= (voxelVals[4] > ContourValue) << 4;
                                    caseIndex |= (voxelVals[5] > ContourValue) << 5;
                                    caseIndex |= (voxelVals[6] > ContourValue) << 6;
                                    caseIndex |= (voxelVals[7] > ContourValue) << 7;
                                }

                                // get list of edges
                                const EDGE_LIST *edge = marchingCubesCases[caseIndex].edges;
                                if (*edge < 0) {
                                    continue;
                                }

                                if (EnableContourAnalysis) {
                                    // remove internal parts
                                    if (polylines[z]->IsInside(x, y, 50) && polylines[z + 1]->IsInside(x, y, 50)) {
                                        continue;
                                    }

                                    // remove noise
                                    if (polylines[z]->FindContour(x, y, 50) == NULL &&
                                        polylines[z]->FindContour(x, y, 75, 4) == NULL && 
                                        !polylines[z + 1]->IsInside(x, y, 60) && (z == 0 || !polylines[z - 1]->IsInside(x, y, 60))) {
                                        continue;
                                    }
                                }

                                // loop through all edges in list
                                while(*edge >= 0) {
                                    // check that memory is not about to be exceeded
                                    if (createCache && (numTrianglesRunningTotal[lod] >= this->TriangleCacheSize[lod])) {
                                        vtkErrorMacro("no. of triangles exceeded allocated cache size\n") ;
                                        break ;
                                    }

                                    // loop through next three edges
                                    for (int ii = 0; ii < 3; ii++, edge++) {
                                        // get corners of unit cube at ends of this edge
                                        // vert0 and vert1 are cube indices of corners (see static consts in .h)
                                        // vertIndeces points to xyz offsets
                                        // eg if *edge = 5, vert0 = 5, vert1 = 6, vertIndeces = {{0,0,1}, {1,0,1}}
                                        const int vert0 = edgeOffsets[*edge][0];
                                        const int vert1 = edgeOffsets[*edge][1];
                                        const int *vertIndeces[2] = {unitCubeVertsXYZ[vert0], unitCubeVertsXYZ[vert1] };

                                        // was this edge created?
                                        // define a reference to pointLocator for the current edge
                                        // reminder: float* pointLocator[][][][3] allocated, but all pointers initially zero
                                        float *(&pointLocatorRef) = pointLocator[xi + lodxy*vertIndeces[0][0]][yi + lodxy*vertIndeces[0][1]][zi + lodz*vertIndeces[0][2]][edgeAxis[*edge]];

                                        if (pointLocatorRef) {
                                            // if edge already created, just read normals and vertices
                                            normals[0] = pointLocatorRef[0];
                                            normals[1] = pointLocatorRef[1];
                                            normals[2] = pointLocatorRef[2];
                                            vertices[0] = pointLocatorRef[3];
                                            vertices[1] = pointLocatorRef[4];
                                            vertices[2] = pointLocatorRef[5];
                                        } else {
                                            // edge not created, so calculate normals and vertices
                                            // calculate coords of vertex, ie intersection of contour with this edge
                                            vertices[0] = fx + vertIndeces[0][0] * fxSize;
                                            vertices[1] = fy + vertIndeces[0][1] * fySize;
                                            vertices[2] = fz + vertIndeces[0][2] * fzSize;
                                            float edgeRatio = (float)(ContourValue - voxelVals[vert0]) / (voxelVals[vert1] - voxelVals[vert0]);
                                            vertices[edgeAxis[*edge]] += edgeRatio * fxyzSize[edgeAxis[*edge]];

                                            // estimate gradients at vert0 and vert1
                                            // use central difference except at edges
                                            // reminder: x, y and z are indices of voxel in volume
                                            int gradient[2][3];
                                            for (int vi = 0; vi < 2; vi++) {
                                                // calculate pointer to vert0 or vert1
                                                // you can check that this corresponds to (vx,vy,vz) using PointerFromIndices()
                                                const T * const verticePtr = voxelPtr + VoxelOffsets[vi == 0 ? vert0 : vert1];

                                                // gradient in x
                                                const int vx = x + lodxy*vertIndeces[vi][0];    // x index of vertex
                                                if (vx == 0) {
                                                    gradient[vi][0] = (int)(verticePtr[1] - verticePtr[0]) ;
                                                } else if (vx >= lastIndex[0]) {
                                                    gradient[vi][0] = (int)(verticePtr[0] - verticePtr[-1]) ;
                                                } else {
                                                    gradient[vi][0] = ((int)(verticePtr[1] - verticePtr[-1])) >> 1 ;
                                                }

                                                // gradient in y
                                                const int vy = y + lodxy*vertIndeces[vi][1];    // y index of vertex
                                                if (vy == 0) {
                                                    gradient[vi][1] = (int)(verticePtr[rowSize] - verticePtr[0]) ;
                                                } else if (vy >= lastIndex[1]) {
                                                    gradient[vi][1] = (int)(verticePtr[0] - verticePtr[-rowSize]) ;
                                                } else {
                                                    gradient[vi][1] = ((int)(verticePtr[rowSize] - verticePtr[-rowSize])) >> 1 ;
                                                }

                                                // gradient in z
                                                const int vz = z + lodz*vertIndeces[vi][2];     // z index of vertex
                                                if (vz == 0) {
                                                    gradient[vi][2] = (int)(verticePtr[sliceSize] - verticePtr[0]) ;
                                                } else if (vz >= lastIndex[2]) {
                                                    gradient[vi][2] = (int)(verticePtr[0] - verticePtr[-sliceSize]) ;
                                                } else {
                                                    gradient[vi][2] = ((int)(verticePtr[sliceSize] - verticePtr[-sliceSize])) >> 1 ;
                                                }
                                            }

                                            // calculate normals by linear interpolation
                                            normals[0] = (gradient[0][0] + edgeRatio * (gradient[1][0] - gradient[0][0])) * fxSizeNorm;
                                            normals[1] = (gradient[0][1] + edgeRatio * (gradient[1][1] - gradient[0][1])) * fySizeNorm;
                                            normals[2] = (gradient[0][2] + edgeRatio * (gradient[1][2] - gradient[0][2])) * fzSizeNorm;

                                            const float normalLenI = -1.f / sqrt(0.0001f + normals[0] * normals[0] + normals[1] * normals[1] + normals[2] * normals[2]);
                                            normals[0] *= normalLenI;
                                            normals[1] *= normalLenI;
                                            normals[2] *= normalLenI;

                                            // finally set the pointLocator
                                            // (remember that normals and vertices are pointers to the same interleaved array)
                                            pointLocatorRef = normals; // normals precede vertices
                                        }

                                        vertices += 6;    
                                        normals += 6;

                                    } // for ii (next edge in triangle)

                                    numberOfTrianglesInBlock++;

                                    // note running total of triangles
                                    numTrianglesRunningTotal[lod]++ ;

                                } // while(*edge) (next triangle in edge list)

                                this->VoxelsRendered++;

                            } // next xi
                        } // next yi
                    } // next zi

                    // if no caching, render the block now, else wait till whole volume is cached
                    // n.b. loop does both lods only if caching enabled, so can't draw both lods
                    if (!createCache && numberOfTrianglesInBlock != 0){
                        glDrawArrays(GL_TRIANGLES, 0, 3 * numberOfTrianglesInBlock);
                        assert(glGetError() == GL_NO_ERROR);
                    }

                    this->NumberOfTriangles[lod] += numberOfTrianglesInBlock;

                } // nextbxi
            } // next byi
        } // next bzi

    } // next LOD

    // render the cached triangles (level of detail = LODLevel)
    // the arrays were attached to openGL earlier with glInterleavedArrays()
    if (createCache) {
        glDrawArrays(GL_TRIANGLES, 0, 3 * this->NumberOfTriangles[LODLevel]);
        unsigned int errorCode = glGetError() ;
        if (errorCode != GL_NO_ERROR) {
            vtkErrorMacro("GL Crashed: " << std::hex << errorCode << "\n") ;
            assert(glGetError() == GL_NO_ERROR);
        }
    }

    for (int z = 0; z < this->DataDimensions[2]; z++) {
        delete polylines[z];
    }
    delete [] polylines ;

    // this measures the percentage of empty blocks
    this->SkippedVoxelBlocks = 100.f * this->SkippedVoxelBlocks / (this->NumBlocks[2] * this->NumBlocks[1] * this->NumBlocks[0]);

    InitializeRender(false);

    this->Timer->StopTimer();

    // note time to draw
    this->TimeToDraw = (float)this->Timer->GetElapsedTime();
    this->TimeToDrawRMC[LODLevel] = this->TimeToDraw;

    // update running mean of time per triangle
    float tpt = this->TimeToDrawRMC[LODLevel] / (float)this->NumberOfTriangles[LODLevel] ;
    if (this->TimePerTriangle > 0.0) {
        this->TimePerTriangle = 0.9 * this->TimePerTriangle + 0.1 * tpt ;
    } else {
        this->TimePerTriangle = tpt;
    }

    if (this->TimeToDraw < 0.0001f) {
        this->TimeToDraw = 0.0001f;
    }
} // RenderMCubes()






//------------------------------------------------------------------------------
// NMcF New version of CreateMCubes()
//
// The code is basically the same as RenderMCubes() with the rendering and caching
// replaced by output to polydata.
//
// NB This function must not alter the cache data, eg this->NumberofTriangles or this->PrevContourValue
// or DrawCache() will crash.
template<typename T> void vtkMAFContourVolumeMapper::CreateMCubes(int LODLevel, vtkPolyData *polydata, const T *dataPointer)
//------------------------------------------------------------------------------
{
    // select LOD
    if (this->AutoLODCreate) {
        LODLevel = BestLODForCreateMCubes();
    }

    // estimate initial no. of triangles to allocate
    const int estimatedNumberOfTriangles = (this->PrevContourValue == this->ContourValue) ? (this->NumberOfTriangles[LODLevel] + 100) : MaxTrianglesNotOptimized;


    // create arrays for points, triangles and normals.
    // These will be assigned to the polydata at the end.
    vtkPoints *points = vtkPoints::New();
    points->Allocate(estimatedNumberOfTriangles);         // allocates space for this many points (3 floats per point)

    vtkCellArray *triangles = vtkCellArray::New();
    triangles->Allocate(3 * estimatedNumberOfTriangles);  // allocates space for this many vertex indices (vtkIdType's)

    vtkFloatArray *vertexNormals = vtkFloatArray::New();
    vertexNormals->SetNumberOfComponents(3);
    vertexNormals->Allocate(3 * estimatedNumberOfTriangles);    // allocates space for this many floats, so have to multiply by 3 yourself this time

    // define local variables to reduce overheads and improve readability
    const T (*BlockMinMax)[2] = (T (*)[2])this->BlockMinMax;
    T ContourValue = (T)this->ContourValue;

    // process blocks and voxels
    this->SkippedVoxelBlocks = 0;
    this->VoxelsRendered = this->VoxelsSkipped = 0;

    // initialization 
    // (nb: we use CreatedTriangles because changing this->NumberOfTriangles[lod]) would mess up the cache)
    // this->PrevContourValue = this->ContourValue;
    this->CreatedTriangles = 0;

    // no. of voxels per slice and per row
    const int rowSize   = this->DataDimensions[0];
    const int sliceSize = this->DataDimensions[0] * this->DataDimensions[1];


    // create 2 polyline lists
    vtkMAFListOfPolyline2D polylines[2];

    if (EnableContourAnalysis) {
        // assign first slice to polylines[0]
        int z = 0 ;
        this->PrepareContours(z, dataPointer + z*sliceSize, polylines[0]);
    }

    // Calculate voxel increments in xy plane and in z
    // Voxel increment in xy is 1 for lod=0, 2 for lod=1, 4 for lod=2, etc
    // However, z resolution between slices may already be poor,
    // so lodz might be less than lodxy.
    int lodxy, lodz ;
    CalculateLodIncrements(LODLevel, &lodxy, &lodz) ;

    // select cube of offsets corresponding to LOD
    const int *VoxelOffsets = this->VoxelVertIndicesOffsets[LODLevel] ;

    // last xyz in volume
    const int lastIndex[3] = { this->DataDimensions[0] - 1, this->DataDimensions[1] - 1, this->DataDimensions[2] - 1};

    // Set up pointLocator - an array of vertex ID's for every edge in the block.
    // Differences between CreateMCubes() and RenderMCubes():
    // 1) Now contains the polydata index instead of vertex vector
    // 2) Stores current and next slice instead of a block, so that the polydata is properly connected.
    // 3) pointLocator stored as 1D array, accessed by offsets as if it was pL[z][y][x][3]
    // NB although we allocate 1+lodz slices, we will only use slices 0 and lodz.
    const int plDims[3] = {this->DataDimensions[0]+lodxy, this->DataDimensions[1]+lodxy, 1+lodz} ;
    const int plSliceSize = 3 * plDims[0] * plDims[1] ;
    const int plOffsets[3] = {3, 3*plDims[0], plSliceSize} ;
    const int plSliceBytes = plSliceSize * sizeof(vtkIdType) ;
    vtkIdType* const pointLocator = new vtkIdType[plSliceSize * plDims[2]] ;
    vtkIdType* const pointLocatorThisSlice = pointLocator ;
    vtkIdType* const pointLocatorNextSlice = pointLocator + lodz*plOffsets[2] ;

    // initialize everything to undefined
    memset(pointLocatorThisSlice, -1, plSliceBytes);
    memset(pointLocatorNextSlice, -1, plSliceBytes);

    //----------------------------
    // Loop through the slices.
    // NB outer loop needs to be z because pointLocator is based on slices instead of blocks.
    //----------------------------
    for (int z = 0 ;  z < this->DataDimensions[2] - lodz ;  z += lodz) {
        const int bzi = z >> VoxelBlockSizeLog;                                 // calculate block no.
        const int zi = z - bzi*VoxelBlockSize ;                                 // z relative to block
        const float fz = this->VoxelCoordinates[2][z];                          // voxel z coord
        const float fzSize = this->VoxelCoordinates[2][z + lodz] - fz;          // size of LOD voxel                    
        const float fzSizeNorm = 1.f / (this->VoxelCoordinates[2][z + 1] - fz); // normalization factor : size of normal voxel                        
        if (fzSize < 0.0001f) // arbitrary minimum size
            continue;

        // update progress
        this->CreatedTriangles = triangles->GetNumberOfCells();
        this->UpdateProgress(float(z) / (this->DataDimensions[2] - 1));

        // reset point locator: copy next slice to current slice and reset next slice
        memcpy(pointLocatorThisSlice, pointLocatorNextSlice, plSliceBytes);
        memset(pointLocatorNextSlice, -1, plSliceBytes);

        // prepare contours
        if (EnableContourAnalysis) {
            // assign next slice to polylines[plI} where plI alternates between 0 and 1
            int plI = ((z+lodz)/lodz) % 2 ;
            this->PrepareContours(z + lodz, dataPointer + (z + lodz)*sliceSize, polylines[plI]);
        }

        //----------------------------
        // loop through the blocks
        //----------------------------
        for (int byi = 0; byi < this->NumBlocks[1]; byi++) {
            // calculate block size in y
            const int yblockSize = (((byi + 1) < this->NumBlocks[1]) ? VoxelBlockSize : (this->DataDimensions[1] - 1 - (byi << VoxelBlockSizeLog)));

            for (int bxi = 0; bxi < this->NumBlocks[0]; bxi++) {
                // calculate block size in x
                const int xblockSize = (((bxi + 1) < this->NumBlocks[0]) ? VoxelBlockSize : (this->DataDimensions[0] - 1 - (bxi << VoxelBlockSizeLog)));

                // calculate block no.
                const int block = bxi + this->NumBlocks[0] * (byi + bzi * this->NumBlocks[1]);

                // skip the block if any of the sizes are zero
                if (xblockSize == 0 || yblockSize == 0) {
                    this->SkippedVoxelBlocks += 1.f;
                    continue;
                } else if ((ContourValue < BlockMinMax[block][0]) || (ContourValue > BlockMinMax[block][1])) {
                    // skip the block if contour value is outside range min-max
                    this->SkippedVoxelBlocks += 1.f;
                    continue;
                }

                //--------------------------------
                // Process the voxels in the block
                //--------------------------------

                // index of first voxel in block
                const int voxelIBlock = VoxelBlockSize * (bzi * sliceSize + byi * rowSize + bxi);

                // initialize voxel count for block
                int numberOfTrianglesInBlock = 0;

                //---------------------------------------------
                // loop over voxels in block
                //---------------------------------------------
                for (int yi = 0; yi < yblockSize; yi += lodxy) {
                    const int   y  = (byi << VoxelBlockSizeLog) + yi;
                    if ((y + lodxy) >= this->DataDimensions[1])
                        continue;
                    const float fy = this->VoxelCoordinates[1][y];
                    const float fySize     = this->VoxelCoordinates[1][y + lodxy] - fy;
                    const float fySizeNorm = 1.f / (this->VoxelCoordinates[1][y + 1] - fy);

                    for (int xi = 0; xi < xblockSize; xi += lodxy) {
                        const int x = (bxi << VoxelBlockSizeLog) + xi;
                        if ((x + lodxy) >= this->DataDimensions[0]) {
                            continue;
                        }
                        const float fx = this->VoxelCoordinates[0][x];
                        const float fxSize = this->VoxelCoordinates[0][x + lodxy] - fx;
                        const float fxSizeNorm = 1.f / (this->VoxelCoordinates[0][x + 1] - fx);

                        const int voxelI = voxelIBlock + zi * sliceSize + yi * rowSize + xi;    // index of current voxel
                        const T * const voxelPtr = dataPointer + voxelI;                 // pointer to current voxel
                        const float fxyzSize[3] = { fxSize, fySize, fzSize };                   // size (LOD size) of current voxel


                        // Find the marching cubes case

                        // get value of voxel and neighbors (VoxelOffsets was set earlier to normal or LOD sized cube)
                        const T voxelVals[8] = {voxelPtr[0], voxelPtr[VoxelOffsets[1]], voxelPtr[VoxelOffsets[2]], voxelPtr[VoxelOffsets[3]],
                            voxelPtr[VoxelOffsets[4]], voxelPtr[VoxelOffsets[5]], voxelPtr[VoxelOffsets[6]], voxelPtr[VoxelOffsets[7]]};

                        // calculate case, each bit of caseIndex is set if corner > contour value
                        int caseIndex = voxelVals[0] > ContourValue; 
                        if(MAXScalar==ContourValue)
                        {
                            // This allows you to get contour at max of range, else can't get last slice in test volume (Matteo 27.06.06) 
                            caseIndex = voxelVals[0] != ContourValue;
                            caseIndex |= (voxelVals[1] != ContourValue) << 1;
                            caseIndex |= (voxelVals[2] != ContourValue) << 2;
                            caseIndex |= (voxelVals[3] != ContourValue) << 3;
                            caseIndex |= (voxelVals[4] != ContourValue) << 4;
                            caseIndex |= (voxelVals[5] != ContourValue) << 5;
                            caseIndex |= (voxelVals[6] != ContourValue) << 6;
                            caseIndex |= (voxelVals[7] != ContourValue) << 7;
                        } else {
                            caseIndex |= (voxelVals[1] > ContourValue) << 1;
                            caseIndex |= (voxelVals[2] > ContourValue) << 2;
                            caseIndex |= (voxelVals[3] > ContourValue) << 3;
                            caseIndex |= (voxelVals[4] > ContourValue) << 4;
                            caseIndex |= (voxelVals[5] > ContourValue) << 5;
                            caseIndex |= (voxelVals[6] > ContourValue) << 6;
                            caseIndex |= (voxelVals[7] > ContourValue) << 7;
                        }

                        // get list of edges
                        const EDGE_LIST *edge = marchingCubesCases[caseIndex].edges;
                        if (*edge < 0) {
                            continue;
                        }

                        if (EnableContourAnalysis) {
                            // remove internal parts
                            if (polylines[0].IsInside(x, y, 50) && polylines[1].IsInside(x, y, 50))
                                continue;
                        }


                        // loop through all edges in list
                        while(*edge >= 0) {
                            // storage for the triangle we are about to construct
                            float vertx[3] ;
                            float norml[3] ;
                            vtkIdType PolyDataIndex[3] ;
                            int vertexno = 0 ;

                            // loop through next three edges
                            for (int ii = 0; ii < 3; ii++, edge++) {
                                // get corners of cube at ends of this edge
                                // vert0 and vert1 are cube indices of corners (see static consts in .h)
                                // vertIndeces points to offsets
                                // eg if *edge = 5, vert0 = 5, vert1 = 6, vertIndeces ={{0,0,1}, {1,0,1}}
                                const int vert0 = edgeOffsets[*edge][0];
                                const int vert1 = edgeOffsets[*edge][1];
                                const int *vertIndeces[2] = {unitCubeVertsXYZ[vert0], unitCubeVertsXYZ[vert1] };

                                // was this edge created?
                                // define a reference to pointLocator for the current edge
                                // reminder: vtkIdType pointLocator[z][x][y][3] with undefined values = -1
                                vtkIdType &pointLocatorRef = pointLocator[
                                    plOffsets[2] * lodz*vertIndeces[0][2] + 
                                        plOffsets[1] * (y + lodxy*vertIndeces[0][1]) + 
                                        plOffsets[0] * (x + lodxy*vertIndeces[0][0]) + 
                                        edgeAxis[*edge]] ;

                                if (pointLocatorRef >= 0) {
                                    // If edge already created, we've had this vertex before.
                                    // Don't want to save the vertex and normal again - just the index (cf RenderMCubes)
                                    // The index will be added to the current cell when we've finished the triangle
                                    PolyDataIndex[vertexno++] = pointLocatorRef ;
                                } else {
                                    // edge not created, so calculate normals and vertices
                                    // calculate coords of vertex, ie intersection of contour with this edge
                                    vertx[0] = fx + vertIndeces[0][0] * fxSize;
                                    vertx[1] = fy + vertIndeces[0][1] * fySize;
                                    vertx[2] = fz + vertIndeces[0][2] * fzSize;
                                    float edgeRatio = (float)(ContourValue - voxelVals[vert0]) / (voxelVals[vert1] - voxelVals[vert0]);
                                    vertx[edgeAxis[*edge]] += edgeRatio * fxyzSize[edgeAxis[*edge]];

                                    // estimate gradients at vert0 and vert1
                                    // use central difference except at edges
                                    // reminder: x, y and z are indices of voxel in volume
                                    int gradient[2][3];
                                    for (int vi = 0; vi < 2; vi++) {
                                        // calculate pointer to vert0 or vert1
                                        // you can check that this corresponds to (vx,vy,vz) using PointerFromIndices()
                                        const T * const verticePtr = voxelPtr + VoxelOffsets[vi == 0 ? vert0 : vert1];

                                        // gradient in x
                                        const int vx = x + lodxy*vertIndeces[vi][0];    // x index of vertex
                                        if (vx == 0) {
                                            gradient[vi][0] = (int)(verticePtr[1] - verticePtr[0]) ;
                                        } else if (vx >= lastIndex[0]) {
                                            gradient[vi][0] = (int)(verticePtr[0] - verticePtr[-1]) ;
                                        } else {
                                            gradient[vi][0] = ((int)(verticePtr[1] - verticePtr[-1])) >> 1 ;
                                        }

                                        // gradient in y
                                        const int vy = y + lodxy*vertIndeces[vi][1];    // y index of vertex
                                        if (vy == 0) {
                                            gradient[vi][1] = (int)(verticePtr[rowSize] - verticePtr[0]) ;
                                        } else if (vy >= lastIndex[1]) {
                                            gradient[vi][1] = (int)(verticePtr[0] - verticePtr[-rowSize]) ;
                                        } else {
                                            gradient[vi][1] = ((int)(verticePtr[rowSize] - verticePtr[-rowSize])) >> 1 ;
                                        }

                                        // gradient in z
                                        const int vz = z + lodz*vertIndeces[vi][2];    // z index of vertex
                                        if (vz == 0) {
                                            gradient[vi][2] = (int)(verticePtr[sliceSize] - verticePtr[0]) ;
                                        } else if (vz >= lastIndex[2]) {
                                            gradient[vi][2] = (int)(verticePtr[0] - verticePtr[-sliceSize]) ;
                                        } else {
                                            gradient[vi][2] = ((int)(verticePtr[sliceSize] - verticePtr[-sliceSize])) >> 1 ;
                                        }
                                    } // (vi) for

                                    // calculate normals by linear interpolation
                                    norml[0] = (gradient[0][0] + edgeRatio * (gradient[1][0] - gradient[0][0])) * fxSizeNorm;
                                    norml[1] = (gradient[0][1] + edgeRatio * (gradient[1][1] - gradient[0][1])) * fySizeNorm;
                                    norml[2] = (gradient[0][2] + edgeRatio * (gradient[1][2] - gradient[0][2])) * fzSizeNorm;

                                    const float normalLenI = -1.f / sqrt(0.0001f + norml[0] * norml[0] + norml[1] * norml[1] + norml[2] * norml[2]);
                                    norml[0] *= normalLenI;
                                    norml[1] *= normalLenI;
                                    norml[2] *= normalLenI;


                                    // save new vertex and normal to polydata
                                    PolyDataIndex[vertexno] = points->InsertNextPoint(vertx) ;
                                    vertexNormals->InsertTuple(PolyDataIndex[vertexno], norml) ;

                                    // finally set the pointLocator to remember the vertex index at this edge
                                    pointLocatorRef = PolyDataIndex[vertexno++]; // normals precede vertices
                                }

                            } // for ii (next edge in triangle)

                            // finished triangle - transfer cell data (ie the vertex indices of the triangle)
                            triangles->InsertNextCell(3, PolyDataIndex) ;
                            numberOfTrianglesInBlock++;

                        } // while(*edge) (next triangle in edge list)

                        this->VoxelsRendered++;

                    } // next xi
                } // next yi

                this->CreatedTriangles += numberOfTrianglesInBlock;

            } // nextbxi
        } // next byi
    } // next z


    for (int plI = 0; plI < 2 ;  plI++) {
        polylines[plI].clear() ;
    }

    // this measures the percentage of empty blocks
    this->SkippedVoxelBlocks = 100.f * this->SkippedVoxelBlocks / (this->NumBlocks[2] * this->NumBlocks[1] * this->NumBlocks[0]);

    // mafLogMessage("CreateMCubes: cv = %f lod = %d triangles = %d", this->ContourValue, LODLevel, this->CreatedTriangles) ;
    // mafLogMessage("CreateMCubes: no. of pts = %d", points->GetNumberOfPoints()) ;


    // free unused memory (beware of memory leaks if ref count is not 1)
    points->Squeeze();
    vertexNormals->Squeeze();
    triangles->Squeeze();

    // assign points, triangles and normals to polydata
    polydata->SetPoints(points);
    polydata->SetPolys(triangles);
    polydata->GetPointData()->SetNormals(vertexNormals);

    // delete local objects (removes reference but doesn't delete from polydata)
    points->Delete() ;
    vertexNormals->Delete() ;
    triangles->Delete() ;

    delete [] pointLocator ;

    this->UpdateProgress(1.f);

} 

//------------------------------------------------------------------------------
// This function returns the index increments in xy and z given the lod index
// For lod = 0,1,2,3... lodxy = 2^n = 1,2,4,8...
// However, the resolution in z, between slice planes, may already be poor, so
// lodz <= lodx such that z resolution is not worse than x resolution.
void vtkMAFContourVolumeMapper::CalculateLodIncrements(int lod, int *lodxy, int *lodz) const
//------------------------------------------------------------------------------
{
    // set lodxy and lodz to 2^lod
    *lodxy = 1 << lod ;
    *lodz = *lodxy ;

    // divide lodz by 2 until z resolution is <= xy resolution
    while(*lodz*this->DataSpacing[2] > *lodxy*this->DataSpacing[0] && *lodz > 1) {
        *lodz >>= 1 ;
    }
}

//------------------------------------------------------------------------------
// Return vertices of voxel cube as offsets to indices in data array */
//               ___
//     ^ y     /|3 2|
//     |   x  / |0 1|
//     /-->  / / ---
//    /     /___ / /
//   / z    |7 6| /
//          |4 5|/
//           ---

void vtkMAFContourVolumeMapper::CalculateVoxelVertIndicesOffsets(int lod, int *offset) const
//------------------------------------------------------------------------------
{
    int lodxy, lodz ;
    CalculateLodIncrements(lod, &lodxy, &lodz) ;

    // calculate voxel offsets for the LOD cube
    int sliceDimension = this->DataDimensions[0] * this->DataDimensions[1] ;
    offset[0] = 0 ;
    offset[1] = lodxy ;
    offset[2] = lodxy * (this->DataDimensions[0] + 1) ;
    offset[3] = lodxy * this->DataDimensions[0] ;
    offset[4] = offset[0] + lodz*sliceDimension;
    offset[5] = offset[1] + lodz*sliceDimension;
    offset[6] = offset[2] + lodz*sliceDimension;
    offset[7] = offset[3] + lodz*sliceDimension;
}


//------------------------------------------------------------------------------
// Calculate volume of voxel given lod
// useful for estimating times or no. of triangles from one lod to another
int vtkMAFContourVolumeMapper::VoxelVolume(int lod) const
//------------------------------------------------------------------------------
{
    int lodxy, lodz ;
    CalculateLodIncrements(lod, &lodxy, &lodz) ;

    return lodxy*lodxy*lodz ;
}

//------------------------------------------------------------------------------
// Estimate the number of triangles for the given LOD using fraction of relevant volume.
// EstimateRelevantVolume() is only called the first time the contour value changes,
// after which there is no further time cost.
int vtkMAFContourVolumeMapper::EstimateTrianglesFromRelevantVolume(int lod)
//------------------------------------------------------------------------------
{
    static float relevantBlocks = -1.0 ;        // remember the result of EstimateRelevantVolume()
    static float lastContour = VTK_FLOAT_MAX ;  // remember the contour value last time

    // reset stored value to undefined if contour value changed since last calculation
    if (lastContour != this->ContourValue) {
        relevantBlocks = -1.0 ;
    }

    if (relevantBlocks < 0) {
        // if no stored value, calculate no. of relevant blocks
        float fracblocks = EstimateRelevantVolume(this->ContourValue) ;
        relevantBlocks = fracblocks * (float)(this->NumBlocks[0] * this->NumBlocks[1] * this->NumBlocks[2]) ;

        // note contour value for which calculations were done
        lastContour = this->ContourValue ;
    }

    // estimate triangles from fraction of blocks containing contour
    float voxperblock = VoxelsInBlock / VoxelVolume(lod) ;
    int numTriangles = (int)(triangles_per_voxel * voxperblock * relevantBlocks) ;

    // return value
    return numTriangles;
}

//------------------------------------------------------------------------------
// Return an estimate of the expected number of triangles for the given LOD.
// Only use this to help decide whether to render at this LOD or not.
int vtkMAFContourVolumeMapper::EstimateNumberOfTriangles(int lod)
//------------------------------------------------------------------------------
{
    if ((this->NumberOfTriangles[lod] >= 0) && (this->PrevContourValue == this->ContourValue)) {
        // no. of triangles is already valid for this contour value, so just return it
        return this->NumberOfTriangles[lod] ;
    } else {
        // try to find highest resolution for which there is a known no. of triangles
        int lodj ;
        for (lodj = 0 ;  lodj < NumberOfLods ;  lodj++){
            if ((this->NumberOfTriangles[lodj] >= 0) && (this->PrevContourValue == this->ContourValue)) {
                break;
            }
        }

        if (lodj < NumberOfLods) {
            // got value for this contour value, different lod
            // estimate time assuming triangles is in inverse proportion to the voxel volume
            float sizefactor = (float)VoxelVolume(lodj) / (float)VoxelVolume(lod) ;
            return (int)(sizefactor * (float)NumberOfTriangles[lodj]) ;
        } else{
            // estimate triangles from fraction of blocks containing contour
            return EstimateTrianglesFromRelevantVolume(lod) ;
        }
    }
}

//------------------------------------------------------------------------------
// Return an estimate of the time to draw using DrawCache()
// Only use this to help decide whether to render at this LOD or not.
float vtkMAFContourVolumeMapper::EstimateTimeToDrawDC(int lod) const
//------------------------------------------------------------------------------
{
    // Note: Rendering is performed either by DrawCache() or RenderMCubes().
    // If time already exists for DrawCache() at this lod, return it.
    // If time exists for DrawCache() at another lod, estimate time assuming inverse proportion to voxel volume 
    // If time exists for RenderMCubes(), estimate, assuming ratio this->TimeCacheToMCubesRatio.
    // If no data, return v large value.

    if ((this->TimeToDrawDC[lod] >= 0.0) && (this->PrevContourValue == this->ContourValue)) {
        // time data already valid for this contour value, so just return it
        return this->TimeToDrawDC[lod] ;
    } else{
        // find highest resolution for which there is a known time with DrawCache()
        int loddc ;
        for (loddc = 0 ;  loddc < NumberOfLods ;  loddc++){
            if ((this->TimeToDrawDC[loddc] >= 0.0) && (this->PrevContourValue == this->ContourValue)) {
                break;
            }
        }

        // find highest resolution for which there is a known time with RenderMCubes()
        int lodrmc ;
        for (lodrmc = 0 ;  lodrmc < NumberOfLods ;  lodrmc++){
            if ((this->TimeToDrawRMC[lodrmc] >= 0.0) && (this->PrevContourValue == this->ContourValue)) {
                break;
            }
        }

        float sizefactor ;
        if (loddc < NumberOfLods) {
            // estimate time assuming time is in inverse proportion to the voxel volume
            sizefactor = (float)VoxelVolume(loddc) / (float)VoxelVolume(lod) ;
            return sizefactor * TimeToDrawDC[loddc] ;
        } else if (lodrmc < NumberOfLods){
            // no time data from DrawCache(), but can estimate from RenderMCubes() timing
            sizefactor = TimeCacheToMCubesRatio * (float)VoxelVolume(lodrmc) / (float)VoxelVolume(lod) ;
            return sizefactor * TimeToDrawRMC[lodrmc] ;
        } else{
            // no valid data from DrawCache() or RenderMCubes() at any lod, so return a very large number
            return VTK_FLOAT_MAX ;
        }
    }
}

//------------------------------------------------------------------------------
// Return an estimate of the time to draw using RenderMCubes()
// Only use this to help decide whether to render at this LOD or not.
float vtkMAFContourVolumeMapper::EstimateTimeToDrawRMC(int lod)
//------------------------------------------------------------------------------
{
    // Note: Rendering is performed either by DrawCache() or RenderMCubes().
    // If time already exists for RenderMCubes() at this lod, return it.
    // If time exists for RenderMCubes() at another lod, estimate time assuming inverse proportion to voxel volume 
    // If no data, estimate time from fraction of relevant blocks

    if ((this->TimeToDrawRMC[lod] >= 0.0) && (this->PrevContourValue == this->ContourValue)) {
        // time data already valid for this contour value, so just return it
        return this->TimeToDrawRMC[lod] ;
    } else {
        // try to find highest resolution for which there is a known time with RenderMCubes()
        int lodrmc ;
        for (lodrmc = 0 ;  lodrmc < NumberOfLods ;  lodrmc++) {
            if ((this->TimeToDrawRMC[lodrmc] >= 0.0) && (this->PrevContourValue == this->ContourValue)) {
                break;
            }
        }

        if (lodrmc < NumberOfLods) {
            // got value for this contour value, different lod
            // estimate time assuming time is in inverse proportion to the voxel volume
            float sizefactor = (float)VoxelVolume(lodrmc) / (float)VoxelVolume(lod);
            return sizefactor * TimeToDrawRMC[lodrmc] ;
        } else if (this->TimePerTriangle > 0.0) {
            // No valid data at any lod, so estimate no. of triangles and guess from there.
            // n.b. it doesn't matter if another function has already called this - there is no time cost.
            int numTriangles = EstimateTrianglesFromRelevantVolume(lod);
            return this->TimePerTriangle * (float)numTriangles ;
        } else {
            // haven't even got a time per triangle value yet - just return a large number
            return VTK_FLOAT_MAX ;
        }
    }
}

//------------------------------------------------------------------------------
// Return highest resolution (LOD) which DrawCache() would be able to draw
// This only checks time and triangles - not whether cache exists yet
// Used to decide whether to call DrawCache() or RenderMCubes()
int vtkMAFContourVolumeMapper::BestLODForDrawCache(vtkRenderer *renderer)
//------------------------------------------------------------------------------
{
    // return level zero if auto lod not enabled
    if (!this->AutoLODRender) {
        return 0;
    }

    // return highest lod with acceptable no. of triangles and time to draw
    for (int lod = 0 ;  lod < NumberOfLods ;  lod++) {
        if (EstimateNumberOfTriangles(lod) < MaxTrianglesNotOptimized &&
            EstimateTimeToDrawDC(lod) < renderer->GetAllocatedRenderTime()) {
            return lod;
        }
    }

    // nothing valid - return lowest level
    return NumberOfLods - 1 ;
}

//------------------------------------------------------------------------------
// Return highest resolution (LOD) which RenderMCubes() can draw
// Used to decide which LOD to render
int vtkMAFContourVolumeMapper::BestLODForRenderMCubes(vtkRenderer *renderer)
//------------------------------------------------------------------------------
{
    // return level zero if auto lod not enabled
    if (!this->AutoLODRender) {
        return 0;
    }

    // return highest lod with acceptable no. of triangles and time to draw
    for (int lod = 0 ;  lod < NumberOfLods ;  lod++) {
        if (EstimateNumberOfTriangles(lod) < MaxTrianglesNotOptimized &&
            EstimateTimeToDrawRMC(lod) < renderer->GetAllocatedRenderTime()) {
            return lod;
        }
    }

    // nothing valid - return lowest level
    return NumberOfLods - 1;
}

//------------------------------------------------------------------------------
// Return highest resolution (LOD) which CreateMCubes() can extract
// Used to decide which LOD to create
int vtkMAFContourVolumeMapper::BestLODForCreateMCubes()
//------------------------------------------------------------------------------
{
    // return level zero if auto lod not enabled
    if (!this->AutoLODCreate) {
        return 0;
    }

    // return highest lod with acceptable no. of triangles
    for (int lod = 0 ;  lod < NumberOfLods ;  lod++){
        if (EstimateNumberOfTriangles(lod) < MaxTrianglesNotOptimized) {
            return lod;
        }
    }

    // nothing valid - return lowest level
    return NumberOfLods - 1 ;
}

//------------------------------------------------------------------------------
static const vtkMarchingSquaresLineCases* marchingSquaresCases = vtkMarchingSquaresLineCases::GetCases();

//------------------------------------------------------------------------------
// Call PrepareContoursTemplate() for data type
void vtkMAFContourVolumeMapper::PrepareContours(const int slice, const void *imageData, vtkMAFListOfPolyline2D& polylines)
//------------------------------------------------------------------------------
{
    this->Polylines = &polylines;

    switch (this->GetDataType()) {
    case VTK_CHAR:
        this->PrepareContoursTemplate(slice, (const char*)imageData);
        break;
    case VTK_UNSIGNED_CHAR:
        this->PrepareContoursTemplate(slice, (const unsigned char*)imageData);
        break;
    case VTK_SHORT:
        this->PrepareContoursTemplate(slice, (const short*)imageData);
        break;
    case VTK_UNSIGNED_SHORT:
        this->PrepareContoursTemplate(slice, (const unsigned short*)imageData);
        break;
    }
}

//------------------------------------------------------------------------------
// Prepare contours
template<typename T> void vtkMAFContourVolumeMapper::PrepareContoursTemplate(const int slice, const T *imageData) 
//------------------------------------------------------------------------------
{
    const T (* const BlockMinMax)[2] = (T (*)[2])((T *)this->BlockMinMax + 2 * (slice >> VoxelBlockSizeLog) * this->NumBlocks[0] * this->NumBlocks[1]);
    const T ContourValue = (T)this->ContourValue;
    const int lastXBlock = this->NumBlocks[0] - 1, lastYBlock = this->NumBlocks[1] - 1;
    const int lastXBlockSize = this->DataDimensions[0] - 1 - (lastXBlock << VoxelBlockSizeLog), lastYBlockSize = this->DataDimensions[1] - 1 - (lastYBlock << VoxelBlockSizeLog);

    vtkMAFListOfPolyline2D& polylines = *this->Polylines;
    polylines.clear();

    int statCounter = 0;

    for (int byi = 0, yblock = 0; byi < this->NumBlocks[1]; byi++, yblock += this->NumBlocks[0]) {
        const int yblockSize = (byi < lastYBlock) ? VoxelBlockSize : lastYBlockSize;
        for (int bxi = 0, block = yblock; bxi < this->NumBlocks[0]; bxi++, block++) {
            if (ContourValue > BlockMinMax[block][1] || ContourValue < BlockMinMax[block][0])
                continue; // skip the block
            const int xblockSize = (bxi < lastXBlock) ? VoxelBlockSize : lastXBlockSize;

            const int xBlock = (bxi << VoxelBlockSizeLog);
            const int yBlock = (byi << VoxelBlockSizeLog);
            const T *imageBlock = imageData + yBlock * this->DataDimensions[0] + xBlock;

            // build the contours
            for (int yi = 0; yi < yblockSize; yi++, imageBlock += this->DataDimensions[0]) {
                for (int xi = 0; xi < xblockSize; xi++) {
                    const T * const voxelPtr = imageBlock + xi;

                    const T voxelVals[4] = {voxelPtr[0], voxelPtr[1], voxelPtr[this->DataDimensions[0] + 1], voxelPtr[this->DataDimensions[0]]};
                    int caseIndex = voxelVals[0] > ContourValue; 
                    caseIndex |= (voxelVals[1] > ContourValue) << 1;
                    caseIndex |= (voxelVals[2] > ContourValue) << 2;
                    caseIndex |= (voxelVals[3] > ContourValue) << 3;
                    const EDGE_LIST * edge = marchingSquaresCases[caseIndex].edges;
                    if (*edge < 0)
                        continue;

                    const int x = (xBlock + xi) << 1;
                    const int y = (yBlock + yi) << 1;

                    vtkMAFPolyline2D::Point line[2];

                    while(*edge >= 0) {
                        static const int edgeToOffset[4][2] = {{1, 0}, {2, 1}, {1, 2}, {0, 1}};
                        // point 1
                        line[0].xy[0] = x + edgeToOffset[*edge][0];
                        line[0].xy[1] = y + edgeToOffset[*edge][1];
                        edge++;
                        // point 2
                        line[1].xy[0] = x + edgeToOffset[*edge][0];
                        line[1].xy[1] = y + edgeToOffset[*edge][1];
                        edge++;

                        // try to add line to polylines
                        const int numOfPolylines = polylines.size();
                        int pi;
                        for (pi = numOfPolylines - 1; pi >= 0; pi--) {
                            if (polylines[pi]->AddNextLine(line))
                                break;
                        }
                        // try to merge polylines
                        if (pi >= 0) {
                            for (int pj = numOfPolylines - 1; pj >= 0; pj--) {
                                if (pi != pj && polylines[pi]->Merge(*polylines[pj])) {
                                    delete polylines[pj];
                                    polylines[pj] = polylines[numOfPolylines - 1];
                                    polylines.pop_back();
                                    break;
                                }
                            }
                            continue;
                        }
                        polylines.push_back(new vtkMAFPolyline2D(line));
                    } // edge loop
                } // for (xi)
            } // for (yi)
        }  // for (bxi)
    } // for (byi)

    for (int pi = polylines.size() - 1; pi >= 0 ; pi--) {
        if (polylines[pi]->Length() < 30) {
            delete polylines[pi];
            polylines[pi] = polylines[polylines.size() - 1];
            polylines.pop_back();
            statCounter++;
        } else {
            polylines[pi]->Close();
        }
    }

    // eliminate inside polygons
#if 0
    for (pi = 0; pi < polylines.size(); pi++) {
        for (int pj = polylines.size() - 1; pj >= 0; pj--) {
            if (pi != pj && polylines[pi]->IsInsideOf(polylines[pj])) {
                delete polylines[pi];
                polylines[pi] = polylines[polylines.size() - 1];
                polylines.pop_back();
                pi--;
                break;
            }
        }
    }
#endif
}

//------------------------------------------------------------------------------
// Get transform matrix for depth calculation
void vtkMAFContourVolumeMapper::CalculateDepthMatrix(double *PM) const
//------------------------------------------------------------------------------
{
    // Get product MP of model-view and projection matrices
    double M[16], P[16] ;

    glGetDoublev(GL_MODELVIEW_MATRIX, M) ;
    glGetDoublev(GL_PROJECTION_MATRIX, P) ;

    vtkMatrix4x4::Multiply4x4(P, M, PM) ;
}

//------------------------------------------------------------------------------
// calculate depth of vertex from screen
float vtkMAFContourVolumeMapper::DepthOfVertex(double *PM, float *v) const
//------------------------------------------------------------------------------
{
    float zhomo, whomo, z ;

    // transform in homogeneous coords PM*v
    zhomo = v[0]*PM[2] + v[1]*PM[6] + v[2]*PM[10] + PM[14] ;
    whomo = v[0]*PM[3] + v[1]*PM[7] + v[2]*PM[11] + PM[15] ;

    // perspective division
    z = zhomo/whomo ;
    return -z ;
}

//------------------------------------------------------------------------------
// Sort triangles into back-to-front order
void vtkMAFContourVolumeMapper::SortTriangles(int lod, bool sortall)
//------------------------------------------------------------------------------
{
    static int isort = 0 ;    // remembers which fraction of the triangles got sorted last time

    // check that we have some triangles to draw
    if (this->TriangleCache[lod] == NULL) {
        return ;
    }

    // Get depth transform matrix
    double PM[16] ;
    CalculateDepthMatrix(PM);

    // allocate list of depth values
    int ntri = this->NumberOfTriangles[lod];
    int nvert = 3*ntri ;
    Idepth *depthlist = new Idepth[ntri];

    // allocate vertex order array, if necessary
    // (deallocated in the destructor)
    if ((this->ordered_vertices[lod] == NULL) || (this->ContourValue != this->PrevContourValue)) {
        // allocate if undefined or no. of triangles has changed
        delete[] ordered_vertices[lod];
        this->ordered_vertices[lod] = new unsigned int[nvert];

        // put array into default order
        for (int j = 0 ;  j < nvert ; j++) {
            this->ordered_vertices[lod][j] = j;
        }
    }

    // loop through triangles and calculate depth of 1st vertex of each triangle
    // store triangles in order of previous sort
    int i, j, jj ;
    float *v, depth ;
    for (i = 0, j = 0 ;  i < ntri ;  i++, j+=3) {
        jj = ordered_vertices[lod][j];             // get index jj of next vertex in depth order
        v = this->TriangleCache[lod] + 6*jj + 3;   // get pointer to vertex jj
        depth = DepthOfVertex(PM, v);              // calculate depth
        depthlist[i].depth = depth;                // store depth and 1st vertex of triangle i
        depthlist[i].index = jj;
    }



    // sort triangles into order
    if (sortall) {
        // sort all the triangles
        std::sort<Idepth*>(depthlist, depthlist+ntri-1);
    } else {
        // sort only a fraction of the triangles each time
        int imax = 2*(SortFraction-1);
        int sortstep = (int)(0.5 * (float)ntri / (float)SortFraction); 
        int j1 = isort * sortstep ;
        int j2 = (ntri-1) - (imax - isort)*sortstep;

        isort++ ;
        if (isort > imax) {
            isort = 0 ;
        }

        std::sort<Idepth*>(depthlist+j1, depthlist+j2);
    }



    // create ordered list of all vertices
    int index ;
    unsigned int *ov = this->ordered_vertices[lod];
    for (i = 0, j = 0 ;  i < ntri ;  i++, j += 3) {
        index = depthlist[i].index;
        ov[j] = index;
        ov[j + 1] = index + 1;
        ov[j + 2] = index + 2;
    }

    delete[] depthlist;
}

//------------------------------------------------------------------------------
// Calculate data pointer from x y z indices
// dataPointer is the pointer to (0,0,0) in the volume
// Useful in debugging to check correspondence between pointer and (x,y,z)
template<typename T> const T* vtkMAFContourVolumeMapper::PointerFromIndices(const T* dataPointer, int x, int y, int z)
//------------------------------------------------------------------------------
{
    int rowSize = this->DataDimensions[0];
    int sliceSize = this->DataDimensions[0] * this->DataDimensions[1];

    return dataPointer + sliceSize*z + rowSize*y + x ;
}

//------------------------------------------------------------------------------
// Calculate x y z indices from data pointer
// dataPointer is the pointer to (0,0,0) in the volume
// Useful in debugging to check correspondence between pointer and (x,y,z)
template<typename T> void vtkMAFContourVolumeMapper::IndicesFromPointer(const T* dataPointer, const T* p, int *x, int *y, int *z)
//------------------------------------------------------------------------------
{
    int rowSize = this->DataDimensions[0];
    int sliceSize = this->DataDimensions[0] * this->DataDimensions[1];

    int diff1 = p-dataPointer;
    *z = diff1 / sliceSize;

    int diff2 = diff1 - sliceSize * (*z);
    *y = diff2 / rowSize;

    *x = diff2 - rowSize * (*y);
}

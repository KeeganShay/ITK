/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit (ITK)
  Module:    itkFEMLoadImageMetric.txx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

=========================================================================*/
#ifndef _itkFEMLoadImageMetric_txx_
#define _itkFEMLoadImageMetric_txx_

#include "itkFEMLoadImageMetric.h"

namespace itk {
namespace fem {

template<class TReference,class TTarget>
LoadImageMetric<TReference , TTarget>::LoadImageMetric()
{
  m_Metric=NULL;
  m_Transform = NULL;
}


template<class TReference,class TTarget>
void LoadImageMetric<TReference , TTarget>::InitializeMetric()
{ 
  if (!m_Transform) m_Transform =  DefaultTransformType::New();
  if (!m_Metric)    m_Metric = DefaultMetricType::New();

//------------------------------------------------------------
// Set up the metric -- see MetricTest in Testing
//------------------------------------------------------------
  
  m_Metric->SetMovingImage( m_RefImage );
  m_Metric->SetFixedImage( m_TarImage );

  typename TargetType::RegionType requestedRegion;
  typename TargetType::SizeType  size;
  typename TargetType::IndexType tindex;
//  typename ReferenceType::IndexType rindex;
  // initialize the offset/vector part
  for( unsigned int k = 0; k < ImageDimension; k++ )
  { 
  //Set the size of the image region
    size[k] = 1;
    tindex[k]=0;
  }

// Set the associated region
  requestedRegion.SetSize(size);
  requestedRegion.SetIndex(tindex);
  m_TarImage->SetRequestedRegion(requestedRegion);  
  m_Metric->SetFixedImageRegion( m_TarImage->GetRequestedRegion() );
  
  m_Metric->SetTransform( m_Transform.GetPointer() );
  m_Interpolator = InterpolatorType::New();
  m_Interpolator->SetInputImage( m_RefImage );
  m_Metric->SetInterpolator( m_Interpolator.GetPointer() );

  
//------------------------------------------------------------
// This call is mandatory before start querying the Metric
// This method do all the necesary connections between the 
// internal components: Interpolator, Transform and Images
//------------------------------------------------------------
  try 
  {
    m_Metric->Initialize();
  } catch( ExceptionObject & e )
  {
    std::cout << "Metric initialization failed" << std::endl;
    std::cout << "Reason " << e.GetDescription() << std::endl;
  }

}


template<class TReference,class TTarget>
LoadImageMetric<TReference , TTarget>::Float 
LoadImageMetric<TReference , TTarget>::EvaluateMetricGivenSolution( Element::ArrayType* el,Float step)
{
  Float energy=0.0; 

  vnl_vector_fixed<Float,ImageDimension> Sol(0.0);  // solution at the local point
  vnl_vector_fixed<Float,ImageDimension> Gpt(0.0);  // global position given by local point
  vnl_vector<Float> Pos(0.0,ImageDimension);  // solution at the point
  
  vnl_vector_fixed<Float,2*ImageDimension> InVec(0.0);
   
  for(  Element::ArrayType::iterator elt=el->begin(); elt!=el->end(); elt++) 
  {
  
    unsigned int sfsz= (*elt)->GetNumberOfNodes();

    vnl_vector<Float> shapeF( sfsz );
    shapeF=(*elt)->ShapeFunctions(Pos);
    
    for (unsigned int ii=0; ii < ImageDimension; ii++)
    { 
      Gpt[ii]=0.0;
      Sol[ii]=0.0; 
      for (unsigned int jj=0; jj<sfsz; jj++)
      {
         vnl_vector<Float> ncoord(2);
         ncoord=(*elt)->GetNodalCoordinates(jj);
         Sol[ii] += shapeF(jj)* GetSolution( (*elt)->GetDegreeOfFreedom(jj*ImageDimension+ii),1); // FIXME ASSUMPTION ABOUT WHERE SOLUTION IS
         Gpt[ii] += shapeF(jj)*ncoord(ii); //FIXME GET COORDINATE AT NODE
      }
      
      InVec[ii]=Gpt[ii];
      InVec[ii+ImageDimension]=Sol[ii];
    }

    //std::cout << " Gpt " << Gpt << " sol " <<  Sol << endl;

    energy+=abs(GetMetric(InVec));
  }
   

  std::cout << " the energy is " << energy << std::endl;

  return energy;

}


template<class TReference,class TTarget>
LoadImageMetric<TReference , TTarget>::VectorType 
LoadImageMetric<TReference , TTarget>::Fg
(LoadImageMetric<TReference , TTarget>::VectorType  InVec) 
{
// We assume the vector input is of size 2*ImageDimension.
// The 0 to ImageDimension-1 elements contain the position, p,
// in the reference image.  The next ImageDimension to 2*ImageDimension-1
// elements contain the value of the vector field at that point, v(p).
//
// Thus, we evaluate the derivative at the point p+v(p) with respect to
// some region of the target (fixed) image by calling the metric with 
// the translation parameters as provided by the vector field at p.
//------------------------------------------------------------
// Set up transform parameters
//------------------------------------------------------------
  int temp=1;
  ParametersType parameters( m_Transform->GetNumberOfParameters() );
  typename TargetType::RegionType requestedRegion;
  typename TargetType::IndexType tindex;
  typename ReferenceType::IndexType rindex;
  VectorType OutVec(ImageDimension,0.0); // gradient direction
  //std::cout << " pos   translation " << InVec  << endl;
   // initialize the offset/vector part
  for( unsigned int k = 0; k < ImageDimension; k++ )
  { 
  //Set the size of the image region
    parameters[k]= InVec[k+ImageDimension]; // this gives the translation by the vector field 
    tindex[k] =InVec[k]+InVec[k+ImageDimension];  // where the piece of reference image currently lines up under the above translation
    rindex[k]= InVec[k];  // position in reference image
  }

// Set the associated region

  requestedRegion.SetSize(m_TarRadius);
  requestedRegion.SetIndex(tindex);

  m_TarImage->SetRequestedRegion(requestedRegion);  
  m_Metric->SetFixedImageRegion( m_TarImage->GetRequestedRegion() );

//--------------------------------------------------------
// Get metric values

  MetricBaseType::MeasureType     measure;
  MetricBaseType::DerivativeType  derivative;

  m_Metric->GetValueAndDerivative( parameters, measure, derivative );
  for( unsigned int k = 0; k < ImageDimension; k++ )
  {
    OutVec[k]= 1.*derivative[k];
  }
 
  //std::cout   << " deriv " << derivative <<  " val " << measure << endl;
 
  return OutVec;
}


template<class TReference,class TTarget>
LoadImageMetric<TReference , TTarget>::Float 
LoadImageMetric<TReference , TTarget>::GetMetric
(LoadImageMetric<TReference , TTarget>::VectorType  InVec) 
{
// We assume the vector input is of size 2*ImageDimension.
// The 0 to ImageDimension-1 elements contain the position, p,
// in the reference image.  The next ImageDimension to 2*ImageDimension-1
// elements contain the value of the vector field at that point, v(p).
//
// Thus, we evaluate the derivative at the point p+v(p) with respect to
// some region of the target (fixed) image by calling the metric with 
// the translation parameters as provided by the vector field at p.
//------------------------------------------------------------
// Set up transform parameters
//------------------------------------------------------------
  ParametersType parameters( m_Transform->GetNumberOfParameters());
  typename TargetType::RegionType requestedRegion;
  typename TargetType::IndexType tindex;
  typename ReferenceType::IndexType rindex;
  VectorType OutVec(ImageDimension,0.0); // gradient direction
  //std::cout << " pos   translation " << InVec  << endl;
   // initialize the offset/vector part
  for( unsigned int k = 0; k < ImageDimension; k++ )
  { 
  //Set the size of the image region
    parameters[k]= InVec[k+ImageDimension]; // this gives the translation by the vector field 
    tindex[k] =InVec[k]+InVec[k+ImageDimension];  // where the piece of reference image currently lines up under the above translation
    rindex[k]= InVec[k];  // position in reference image
  }

// Set the associated region

  requestedRegion.SetSize(m_TarRadius);
  requestedRegion.SetIndex(tindex);

  m_TarImage->SetRequestedRegion(requestedRegion);  
  m_Metric->SetFixedImageRegion( m_TarImage->GetRequestedRegion() );

//--------------------------------------------------------
// Get metric values

  MetricBaseType::MeasureType     measure;
  MetricBaseType::DerivativeType  derivative;

  m_Metric->GetValueAndDerivative( parameters, measure, derivative );
 
  //std::cout   << " deriv " << derivative <<  " val " << measure << endl;
 
  return (Float) measure;
}



} // end namespace fem
} // end namespace itk

#endif

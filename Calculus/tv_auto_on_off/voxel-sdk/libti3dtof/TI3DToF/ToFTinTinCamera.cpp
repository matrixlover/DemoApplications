/*
 * TI Voxel Lib component.
 *
 * Copyright (c) 2014 Texas Instruments Inc.
 */

#include "ToFTinTinCamera.h"
#include <Configuration.h>
#include <ParameterDMLParser.h>

namespace Voxel
{
  
namespace TI
{
  
/// Custom parameters
class TinTinVCOFrequency: public FloatParameter
{
  ToFTinTinCamera &_depthCamera;
  String _vcoFreq, _modM, _modMFrac, _modN;
public:
  TinTinVCOFrequency(ToFTinTinCamera &depthCamera, RegisterProgrammer &programmer, const String &vcoFreq, const String &modM, const String &modMFrac, const String &modN):
  FloatParameter(programmer, vcoFreq, "MHz", 0, 0, 0, 1, 300, 600, 384, "VCO frequency", 
                 "Frequency of the VCO used for generating modulation frequencies", IOType::IO_READ_WRITE, {modM, modN}), _depthCamera(depthCamera),
                 _vcoFreq(vcoFreq), _modM(modM), _modMFrac(modMFrac), _modN(modN) {}
                 
  virtual bool get(float &value, bool refresh = false)
  {
    uint modM, modMFrac, modN, systemClockFrequency;
    if(!_depthCamera._get(_modM, modM, refresh) || !_depthCamera._get(_modMFrac, modMFrac, refresh)
      || !_depthCamera._get(_modN, modN, refresh) || !_depthCamera._getSystemClockFrequency(systemClockFrequency))
      return false;
    
    if(modN == 0)
      return false;
    
    float modMf = modM + ((float)modMFrac)/(1 << 16);
    
    float v = systemClockFrequency*modMf/modN;
    
    if(!validate(v))
      return false;
    
    value = v;
    return true;
  }
  
  virtual bool set(const float &value)
  {
    if(!validate(value))
      return false;
    
    if(!_depthCamera._set(MOD_PLL_UPDATE, true))
      return false;
    
    ParameterPtr pllUpdate(nullptr, [this](Parameter *) { _depthCamera._set(MOD_PLL_UPDATE, false); }); // Set PLL update to false when going out of scope of this function
    
    uint modM, modMFrac, modN, systemClockFrequency;
    if(!_depthCamera._getSystemClockFrequency(systemClockFrequency))
      return false;
    
    modN = 2;
    
    if(systemClockFrequency == 0)
      return false;
    
    float modMf = value*modN/systemClockFrequency;
    modM = (uint)modMf;
    modMFrac = (uint)((modMf - modM)*(1 << 16));
    
    if(!_depthCamera._set(_modM, modM) || !_depthCamera._set(_modN, modN) || !_depthCamera._set(_modMFrac, modMFrac))
      return false;
    
    _value = value;
    
    return true;
  }
  
  virtual ~TinTinVCOFrequency() {}
};

class TinTinModulationFrequencyParameter: public FloatParameter
{
  ToFTinTinCamera &_depthCamera;
  String _modPS, _vcoFreq;
public:
  TinTinModulationFrequencyParameter(ToFTinTinCamera &depthCamera, RegisterProgrammer &programmer, const String &name, const String &vcoFreq, const String &modPS):
  FloatParameter(programmer, name, "MHz", 0, 0, 0, 1, 2.5f, 600.0f, 18, "Modulation frequency", "Frequency used for modulation of illumination", 
                 Parameter::IO_READ_WRITE, {vcoFreq, modPS}), _vcoFreq(vcoFreq), _modPS(modPS), _depthCamera(depthCamera) {}
                 
  virtual const float lowerLimit() const 
  { 
    uint quadCount;
    
    if(!_depthCamera._get(QUAD_CNT_MAX, quadCount))
      return _lowerLimit;
    
    if(quadCount == 0)
      return _lowerLimit;
    
    return 37.5f/quadCount; 
    
  }
  virtual const float upperLimit() const 
  { 
    uint quadCount;
    
    if(!_depthCamera._get(QUAD_CNT_MAX, quadCount))
      return _upperLimit;
    
    if(quadCount == 0)
      return _upperLimit;
    
    return 600.0f/quadCount;
  }
                 
  virtual bool get(float &value, bool refresh = false)
  {
    float vcoFrequency;
    
    uint modulationPS;
    
    uint quadCount;
    
    if(!_depthCamera._get(_vcoFreq, vcoFrequency, refresh) || !_depthCamera._get(_modPS, modulationPS, refresh) || !_depthCamera._get(QUAD_CNT_MAX, quadCount, refresh))
      return false;
    
    if(quadCount == 0)
      return false;
    
    float v = vcoFrequency/quadCount/(1 + modulationPS);
    
    if(!validate(v))
      return false;
    
    value = v;
    return true;
  }
  
  virtual bool set(const float &value)
  {
    if(!validate(value))
      return false;
    
    ParameterPtr p = _depthCamera.getParam(_vcoFreq);
    
    uint quadCount;
    
    if(!p || !_depthCamera._get(QUAD_CNT_MAX, quadCount))
      return false;
    
    if(quadCount == 0)
      return false;
    
    TinTinVCOFrequency &v = (TinTinVCOFrequency &)*p;
    
    uint modulationPS = v.upperLimit()/quadCount/value - 1;
    
    if(!v.set((modulationPS + 1)*quadCount*value))
      return false;
    
    if(!_depthCamera._set(MOD_PLL_UPDATE, true))
      return false;
    
    ParameterPtr pllUpdate(nullptr, [this](Parameter *) { _depthCamera._set(MOD_PLL_UPDATE, false); }); // Set PLL update to false when going out of scope of this function
    
    if(!_depthCamera._set(_modPS, modulationPS))
      return false;
    
    float val;
    
    if(!v.get(val))
      return false;
    
    return true;
  }
  
  virtual ~TinTinModulationFrequencyParameter() {}
};

ToFTinTinCamera::ToFTinTinCamera(const String &name, DevicePtr device): ToFCamera(name, device)
{
}

bool ToFTinTinCamera::_getSystemClockFrequency(uint &frequency) const
{
  frequency = 48;
  return true;
}


bool ToFTinTinCamera::_getMaximumFrameSize(FrameSize &s) const
{
  s.width = 320;
  s.height = 240;
  return true;
}

  
bool ToFTinTinCamera::_init()
{
  Configuration c;
  
  String name = configFile.get("core", "dml");
  
  if(!name.size() || !c.getConfFile(name)) // true => name is now a proper path
  {
    logger(LOG_ERROR) << "ToFTinTinCamera: Failed to locate/read DML file '" << name << "'" << std::endl;
    return false;
  }
  
  ParameterDMLParser p(*_programmer, name);
  
  Vector<ParameterPtr> params;
  
  if(!p.getParameters(params))
  {
    logger(LOG_ERROR) << "ToFTinTinCamera: Could not read parameters from DML file '" << name << "'" << std::endl;
    return _parameterInit = false;
  }
  
  for(auto &p: params)
  {
    if((p->address() >> 8) == 0) // bankId == 0
      p->setAddress((0x58 << 8) + p->address());
    else if((p->address() >> 8) == 3) // bankId == 3
      p->setAddress((0x5C << 8) + (p->address() & 0xFF));
  }
  
  if(!_addParameters(params))
    return false;
  
  
  if(!_addParameters({
    ParameterPtr(new TinTinVCOFrequency(*this, *_programmer, VCO_FREQ1, MOD_M1, MOD_M_FRAC1, MOD_N1)),
    ParameterPtr(new TinTinVCOFrequency(*this, *_programmer, VCO_FREQ2, MOD_M2, MOD_M_FRAC2, MOD_N2)),
    ParameterPtr(new TinTinModulationFrequencyParameter(*this, *_programmer, MOD_FREQ1, VCO_FREQ1, MOD_PS1)),
    ParameterPtr(new TinTinModulationFrequencyParameter(*this, *_programmer, MOD_FREQ2, VCO_FREQ2, MOD_PS2)),
  }))
    return false;
  
  if(!ToFCamera::_init())
  {
    return false;
  }
  
  return true;
}

bool ToFTinTinCamera::_reset()
{
  if(!ToFCamera::_reset() || !set(PHASE_CORR_1, configFile.getInteger("calib", "phase_corr_1")))
    return false;
  return true;
}


bool ToFTinTinCamera::_initStartParams()
{
  if(
    !set(TG_DISABLE, true) ||
    !_programmer->writeRegister(0x583A, 0x008000) ||
    !_programmer->writeRegister(0x583A, 0x004000) ||
    !set(BLK_SIZE, 1024U) ||
    !set(BLK_HEADER_EN, true) ||
    !set(OP_CS_POL, true) ||
    !set(FB_READY_EN, true) ||
    !set(CONFIDENCE_THRESHOLD, 1U) ||
    //!set(DEBUG_EN, true) || // Uncomment this for sample data
    !set(TG_DISABLE, false) ||
    !set(MOD_PLL_UPDATE, true) ||
    !set(MOD_PLL_UPDATE, false))
      return false;
      
  return ToFCamera::_initStartParams();
}

bool ToFTinTinCamera::_allowedROI(String &message)
{
  message  = "Column start and end must be multiples of 16, both between 0 to 319. ";
  message += "Row start and end must be between 0 to 239.";
  return true;
}

bool ToFTinTinCamera::_getROI(RegionOfInterest &roi)
{
  uint rowStart, rowEnd, colStart, colEnd;
  
  if(!_get(ROW_START, rowStart) || !_get(ROW_END, rowEnd) || !_get(COL_START, colStart) || !_get(COL_END, colEnd))
  {
    logger(LOG_ERROR) << "ToFTinTinCamera: Could not get necessary parameters for ROI." << std::endl;
    return false;
  }
  
  colStart = colStart*16;
  colEnd = (colEnd + 1)*16 - 1;
  
  roi.x = colStart;
  roi.y = rowStart;
  roi.width = colEnd - colStart + 1;
  roi.height = rowEnd - rowStart + 1;
  return true;
}

bool ToFTinTinCamera::_setROI(const RegionOfInterest &roi)
{
  if(isRunning())
  {
    logger(LOG_ERROR) << "ToFTinTinCamera: Cannot set frame size while the camera is streaming" << std::endl;
    return false;
  }
  
  uint rowStart, rowEnd, colStart, colEnd;
  
  colStart = (roi.x/16);
  colEnd = ((roi.x + roi.width)/16) - 1;
  
  rowStart = roi.y;
  rowEnd = rowStart + roi.height - 1;
  
  if(!_set(ROW_START, rowStart) || !_set(ROW_END, rowEnd) || !_set(COL_START, colStart) || !_set(COL_END, colEnd))
  {
    logger(LOG_ERROR) << "ToFTinTinCamera: Could not get necessary parameters for ROI." << std::endl;
    return false;
  }
  
  if(!_setBinning(1, 1, roi))
  {
    logger(LOG_ERROR) << "ToFTintinCamera: Could not reset binning while setting ROI." << std::endl;
    return false;
  }
  
  FrameSize s;
  if(!_getFrameSize(s) || !_setFrameSize(s, false)) // Get and set frame size to closest possible
  {
    logger(LOG_ERROR) << "ToFTinTinCamera: Could not update frame size to closest valid frame size" << std::endl;
    return false;
  }
  
  return true;
}

bool ToFTinTinCamera::_isHistogramEnabled() const
{
  return false;
}

bool ToFTinTinCamera::_getToFFrameType(ToFFrameType &frameType) const
{
  frameType = ToF_PHASE_AMPLITUDE;
  return true;
}


bool ToFTinTinCamera::_getDepthScalingFactor(float &factor)
{
  float modulationFrequency1, modulationFrequency2;
  bool dealiasingEnabled;
  
  uint modulationPS1, modulationPS2, systemClockFrequency;
  
  uint modM1, modM2, modN1, modN2;
  
  if(!get(DEALIAS_EN, dealiasingEnabled))
    return false;
  
  bool frequencyCorrectionPresent = false;
  float frequencyCorrection = 1.0f, frequencyCorrectionAt;
  
  if(configFile.isPresent("calib", "freq_corr"))
  {
    frequencyCorrection = configFile.getFloat("calib", "freq_corr");
    frequencyCorrectionAt = configFile.getFloat("calib", "freq_corr_at");
    
    if(frequencyCorrectionAt < 1E-5)
      frequencyCorrectionPresent = false;
    else
      frequencyCorrectionPresent = true;
  }
  
  if(dealiasingEnabled)
  {
    if(!_getSystemClockFrequency(systemClockFrequency) || !get(MOD_PS1, modulationPS1) || !get(MOD_PS2, modulationPS2) ||
      !get(MOD_M1, modM1) || !get(MOD_M2, modM2) ||
      !get(MOD_N1, modN1) || !get(MOD_N2, modN2))
      return false;
    
    float freq = systemClockFrequency*gcd(modM1*modN2*modulationPS2, modM2*modN1*modulationPS1)/(modN1*modN2*modulationPS1*modulationPS2);
    
    if(frequencyCorrectionPresent)
      freq *= (frequencyCorrection*freq/frequencyCorrectionAt);
    
    factor = SPEED_OF_LIGHT/1E6f/(2*(1 << 12)*freq);
    
    return true;
  }
  else
  {
    if(!get(MOD_FREQ1, modulationFrequency1))
      return false;
    
    if(frequencyCorrectionPresent)
      modulationFrequency1 *= (frequencyCorrection*modulationFrequency1/frequencyCorrectionAt);
    
    factor = SPEED_OF_LIGHT/1E6f/2/modulationFrequency1/(1 << 12);
    return true;
  }
}
 
}
}

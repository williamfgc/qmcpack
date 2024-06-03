//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source License.
// See LICENSE file in top directory for details.
//
// Copyright (c) 2024 QMCPACK developers.
//
// File developed by: Jaron T. Krogel, krogeljt@ornl.gov, Oak Ridge National Laboratory
//
// File created by: Jaron T. Krogel, krogeljt@ornl.gov, Oak Ridge National Laboratory
//////////////////////////////////////////////////////////////////////////////////////


#ifndef QMCPLUSPLUS_WALKERTRACEBUFFER_H
#define QMCPLUSPLUS_WALKERTRACEBUFFER_H


#include <Configuration.h>
#include "OhmmsPETE/OhmmsArray.h"
#include "hdf/hdf_archive.h"

#include <unordered_set>



namespace qmcplusplus
{

const unsigned int WDMAX = 4;
using WTraceInt          = long;
using WTraceReal         = OHMMS_PRECISION_FULL;
using WTraceComp         = std::complex<WTraceReal>;
#ifndef QMC_COMPLEX
using WTracePsiVal = WTraceReal;
#else
using WTracePsiVal = WTraceComp;
#endif



struct WalkerQuantityInfo
{
  std::string name;
  size_t dimension;
  size_t size;
  size_t unit_size;
  TinyVector<size_t, WDMAX> shape;
  size_t buffer_start;
  size_t buffer_end;

  WalkerQuantityInfo(const std::string& name_,size_t unit_size_,size_t buffer_start_,size_t n1=1,size_t n2=0,size_t n3=0,size_t n4=0)
  {
    name         = name_;
    unit_size    = unit_size_;
    buffer_start = buffer_start_;
    shape[0]     = n1;
    shape[1]     = n2;
    shape[2]     = n3;
    shape[3]     = n4;

    dimension = 0;
    size      = 1;
    for (size_t d=0;d<WDMAX;++d)
      if (shape[d]>0)
      {
        dimension++;
        size *= shape[d];
      }
    buffer_end = buffer_start + size*unit_size;
  }
};




template<typename T>
class WalkerTraceBuffer
{
public:
  bool first_collect;
  std::string label;
  hsize_t hdf_file_pointer;

private:
  size_t quantity_pointer;
  std::vector<WalkerQuantityInfo> quantity_info;
  size_t walker_data_size;
  Array<T, 2> buffer;
  hsize_t dims[2];

public:
  WalkerTraceBuffer()
  {
    label             = "?";
    first_collect     = true;
    walker_data_size  = 0;
    quantity_pointer  = 0;
    resetBuffer();
  }

  inline size_t nrows() { return buffer.size(0); }

  inline size_t ncols() { return buffer.size(1); }

  inline void resetBuffer() { buffer.resize(0, buffer.size(1)); }

  inline void resetCollect()
  {
    if(quantity_pointer!=quantity_info.size())
      throw std::runtime_error("WalkerTraceBuffer quantity_pointer has not been moved through all quantities prior during collect() call.");
    first_collect = false;
    quantity_pointer = 0;
  }


  inline bool sameAs(const WalkerTraceBuffer<T>& ref) { return buffer.size(1) == ref.buffer.size(1); }


  inline void collect(const std::string& name, const T& value)
  {
    size_t irow=0;
    if( first_collect )
    {
      WalkerQuantityInfo wqi_(name,1,walker_data_size);
      quantity_info.push_back(wqi_);
      walker_data_size = wqi_.buffer_end;
      resetRowSize(walker_data_size);
    }
    else
    {
      if(quantity_pointer==0)
        makeNewRow();
      irow = buffer.size(0)-1;
    }
    auto& wqi = quantity_info[quantity_pointer];
    buffer(irow,wqi.buffer_start) = value;
    quantity_pointer++;
  }


  template<unsigned D>
  inline void collect(const std::string& name, Array<T,D> arr)
  {
    size_t n1 = arr.size(0);
    size_t n2,n3,n4;
    n2=n3=n4=0;
    if (D>4)
      throw std::runtime_error("WalkerTraceBuffer::collect  Only arrays up to dimension 4 are currently supported.");
    if (D>1) n2 = arr.size(1);
    if (D>2) n3 = arr.size(2);
    if (D>3) n4 = arr.size(3);
    size_t irow=0;
    if( first_collect )
    {
      WalkerQuantityInfo wqi_(name,1,walker_data_size,n1,n2,n3,n4);
      quantity_info.push_back(wqi_);
      walker_data_size = wqi_.buffer_end;
      resetRowSize(walker_data_size);
    }
    else
    {
      if(quantity_pointer==0)
        makeNewRow();
      irow = buffer.size(0)-1;
    }
    auto& wqi = quantity_info[quantity_pointer];
    auto& arr1d = arr.storage();
    for (size_t n=0;n<arr1d.size();++n)
      buffer(irow,wqi.buffer_start+n) = arr1d[n];
    quantity_pointer++;
  }


  template<unsigned D>
  inline void collect(const std::string& name, Array<std::complex<T>,D> arr)
  {
    size_t n1 = arr.size(0);
    size_t n2,n3,n4;
    n2=n3=n4=0;
    if (D>4)
      throw std::runtime_error("WalkerTraceBuffer::collect  Only arrays up to dimension 4 are currently supported.");
    if (D>1) n2 = arr.size(1);
    if (D>2) n3 = arr.size(2);
    if (D>3) n4 = arr.size(3);
    size_t irow=0;
    if( first_collect )
    {
      WalkerQuantityInfo wqi_(name,2,walker_data_size,n1,n2,n3,n4);
      quantity_info.push_back(wqi_);
      walker_data_size = wqi_.buffer_end;
      resetRowSize(walker_data_size);
    }
    else
    {
      if(quantity_pointer==0)
        makeNewRow();
      irow = buffer.size(0)-1;
    }
    auto& wqi = quantity_info[quantity_pointer];
    auto& arr1d = arr.storage();
    size_t n = 0;
    for (size_t i=0;i<arr1d.size();++i)
    {
      buffer(irow,wqi.buffer_start+n) = std::real(arr1d[n]); ++n;
      buffer(irow,wqi.buffer_start+n) = std::imag(arr1d[n]); ++n;
    }
    quantity_pointer++;
  }


  inline void addRow(WalkerTraceBuffer<T> other, size_t i)
  {
    auto& other_buffer = other.buffer;
    if (first_collect)
    {
      resetRowSize(other_buffer.size(1));
      quantity_info = other.quantity_info;
      first_collect = false;
    }
    else
    {
      if(buffer.size(1)!=other_buffer.size(1))
        throw std::runtime_error("WalkerTraceBuffer::add_row  Row sizes must match.");
      makeNewRow();
    }
    size_t ib = buffer.size(0)-1;
    for(size_t j=0;j<buffer.size(1);++j)
      buffer(ib,j) = other_buffer(i,j);
  }


  inline void writeSummary(std::string pad = "  ")
  {
    std::string pad2 = pad  + "  ";
    std::string pad3 = pad2 + "  ";
    app_log() << std::endl;
    app_log() << pad << "WalkerTraceBuffer(" << label << ")" << std::endl;
    app_log() << pad2 << "nrows       = " << buffer.size(0) << std::endl;
    app_log() << pad2 << "row_size    = " << buffer.size(1) << std::endl;
    for(size_t n=0;n<quantity_info.size();++n)
    {
      auto& wqi = quantity_info[n];
      app_log()<<pad2<<"quantity "<< n <<":  "<<wqi.dimension<<"  "<<wqi.size<<"  "<<wqi.unit_size<<"  "<<wqi.buffer_start<<"  "<<wqi.buffer_end<<" ("<<wqi.name<<")"<<std::endl;
    }
    app_log() << pad << "end WalkerTraceBuffer(" << label << ")" << std::endl;
  }

  inline void registerHDFData(hdf_archive& f)
  {
    auto& top = label;
    f.push(top);
    f.push("data_layout");
    for(auto& wqi: quantity_info)
    {
      f.push(wqi.name);
      f.write(wqi.dimension,    "dimension"  );
      f.write(wqi.shape,        "shape"      );
      f.write(wqi.size,         "size"       );
      f.write(wqi.unit_size,    "unit_size"  );
      f.write(wqi.buffer_start, "index_start");
      f.write(wqi.buffer_end,   "index_end"  );
      f.pop();
    }
    f.pop();
    f.pop();
    if (!f.open_groups())
      throw std::runtime_error("WalkerTraceBuffer(" + label + ")::register_hdf_data() some hdf groups are still open at the end of registration");
    hdf_file_pointer = 0;
  }


  inline void writeHDF(hdf_archive& f)
  {
    writeHDF(f,hdf_file_pointer);
  }


  inline void writeHDF(hdf_archive& f, hsize_t& file_pointer)
  {
    auto& top = label;
    dims[0] = buffer.size(0);
    dims[1] = buffer.size(1);
    if (dims[0] > 0)
    {
      f.push(top);
      h5d_append(f.top(), "data", file_pointer, buffer.dim(), dims, buffer.data());
      f.pop();
    }
    f.flush();
  }

private:
  inline void resetRowSize(size_t row_size)
  {
    auto nrows = buffer.size(0);
    if(nrows==0)
      nrows++;
    if(nrows!=1)
      throw std::runtime_error("WalkerTraceBuffer::reset_rowsize  row_size (number of columns) should only be changed during growth of the first row.");
    auto buffer_old(buffer);
    buffer.resize(nrows,row_size);
    std::copy_n(buffer_old.data(), buffer_old.size(), buffer.data());
    if(buffer.size(0)!=1)
      throw std::runtime_error("WalkerTraceBuffer::reset_rowsize  buffer should contain only a single row upon completion.");
    if(buffer.size(1)!=row_size)
      throw std::runtime_error("WalkerTraceBuffer::reset_rowsize  length of buffer row should match the requested row_size following the reset/udpate.");
  }

  inline void makeNewRow()
  {
    size_t nrows    = buffer.size(0);
    size_t row_size = buffer.size(1);
    if (row_size==0)
      throw std::runtime_error("WalkerTraceBuffer::makeNewRow  Cannot make a new row of size zero.");
    nrows++;
    // resizing buffer(type Array) doesn't preserve data. Thus keep old data and copy over
    auto buffer_old(buffer);
    buffer.resize(nrows, row_size);
    std::copy_n(buffer_old.data(), buffer_old.size(), buffer.data());
  }

};


} // namespace qmcplusplus

#endif

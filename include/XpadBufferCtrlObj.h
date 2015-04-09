//###########################################################################
// This file is part of LImA, a Library for Image Acquisition
//
// Copyright (C) : 2009-2011
// European Synchrotron Radiation Facility
// BP 220, Grenoble 38043
// FRANCE
//
// This is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//###########################################################################
#ifndef XPADBUFFERCTRLOBJ_H
#define XPADBUFFERCTRLOBJ_H

#include "XpadCamera.h"
#include "lima/HwBufferCtrlObj.h"

namespace lima
{
  namespace Xpad
  {
    //*******************************************************************
    // \class BufferCtrlObj
    // \brief Control object providing Xpad buffering interface
    //*******************************************************************
    class BufferCtrlObj : public HwBufferCtrlObj
    {
        DEB_CLASS_NAMESPC(DebModCamera, "BufferCtrlObj", "Xpad");

    public:
        BufferCtrlObj(Camera&);
        virtual ~BufferCtrlObj();

        virtual void setFrameDim(const FrameDim& frame_dim);
        virtual void getFrameDim(FrameDim& frame_dim);

        virtual void setNbBuffers(int nb_buffers);
        virtual void getNbBuffers(int& nb_buffers);

        virtual void setNbConcatFrames(int nb_concat_frames);
        virtual void getNbConcatFrames(int& nb_concat_frames);

        virtual void getMaxNbBuffers(int& max_nb_buffers);

        virtual void *getBufferPtr(int buffer_nb, int concat_frame_nb = 0);
        virtual void *getFramePtr(int acq_frame_nb);

        virtual void getStartTimestamp(Timestamp& start_ts);
        virtual void getFrameInfo(int acq_frame_nb, HwFrameInfoType& info);

        virtual void registerFrameCallback(HwFrameCallback& frame_cb);
        virtual void unregisterFrameCallback(HwFrameCallback& frame_cb);

    private:
        BufferCtrlMgr& m_buffer_mgr;

    };
  } // namespace Xpad
} // namespace lima

#endif // XPADBUFFERCTRLOBJ_H

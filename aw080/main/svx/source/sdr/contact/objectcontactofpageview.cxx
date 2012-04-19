/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_svx.hxx"
#include <svx/sdr/contact/objectcontactofpageview.hxx>
#include <svx/sdr/contact/viewobjectcontactofunocontrol.hxx>
#include <svx/svdpagv.hxx>
#include <svx/svdpage.hxx>
#include <svx/sdr/contact/displayinfo.hxx>
#include <svx/sdr/contact/viewobjectcontact.hxx>
#include <svx/svdview.hxx>
#include <svx/sdr/contact/viewcontact.hxx>
#include <svx/sdr/animation/objectanimator.hxx>
#include <svx/sdr/event/eventhandler.hxx>
#include <svx/sdrpagewindow.hxx>
#include <svx/sdrpaintwindow.hxx>
#include <drawinglayer/processor2d/vclprocessor2d.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <drawinglayer/primitive2d/transformprimitive2d.hxx>
#include <svx/sdr/contact/objectcontacttools.hxx>
#include <com/sun/star/rendering/XSpriteCanvas.hpp>
#include <svx/unoapi.hxx>

//////////////////////////////////////////////////////////////////////////////

using namespace com::sun::star;

//////////////////////////////////////////////////////////////////////////////

namespace sdr
{
	namespace contact
	{
		// internal access to SdrPage of SdrView
		SdrPage& ObjectContactOfPageView::GetSdrPage() const
		{
			return GetPageWindow().GetPageView().getSdrPageFromSdrPageView();
		}

		ObjectContactOfPageView::ObjectContactOfPageView(SdrPageWindow& rPageWindow)
		:	ObjectContact(),
			mrPageWindow(rPageWindow)
		{
			// init PreviewRenderer flag
			setPreviewRenderer(((SdrPaintView&)rPageWindow.GetPageView().GetView()).IsPreviewRenderer());

			// init timer
			SetTimeout(1);
			Stop();
		}

		ObjectContactOfPageView::~ObjectContactOfPageView()
		{
			// execute missing LazyInvalidates and stop timer
			Timeout();
		}

		// LazyInvalidate request. Take action.
		void ObjectContactOfPageView::setLazyInvalidate(ViewObjectContact& /*rVOC*/)
		{
			// do NOT call parent, but remember that something is to do by
			// starting the LazyInvalidateTimer
			Start();
		}

		// call this to support evtl. preparations for repaint
		void ObjectContactOfPageView::PrepareProcessDisplay()
		{
			if(IsActive())
			{
				static bool bInvalidateDuringPaint(true);

				if(bInvalidateDuringPaint)
				{
					// there are still non-triggered LazyInvalidate events, trigger these
					Timeout();
				}
			}
		}

		// From baseclass Timer, the timeout call triggered by te LazyInvalidate mechanism
		void ObjectContactOfPageView::Timeout()
		{
			// stop the timer
			Stop();

			// invalidate all LazyInvalidate VOCs new situations
			if(!getViewObjectContacts().empty())
			{
				const ViewObjectContactSet::const_iterator aEnd(getViewObjectContacts().end());
				ViewObjectContactSet::iterator aCurrent(getViewObjectContacts().begin());

				for(;aCurrent != aEnd; aCurrent++)
				{
					(*aCurrent)->triggerLazyInvalidate();
				}
			}
		}

		// Process the whole displaying
		void ObjectContactOfPageView::ProcessDisplay(DisplayInfo& rDisplayInfo)
		{
			const SdrPage& rStartPage = GetSdrPage();

			if(!rDisplayInfo.GetProcessLayers().IsEmpty())
			{
				const ViewContact& rDrawPageVC = rStartPage.GetViewContact();

				if(rDrawPageVC.GetObjectCount())
				{
					DoProcessDisplay(rDisplayInfo);
				}
			}

			// after paint take care of the evtl. scheduled asynchronious commands.
			// Do this by resetting the timer contained there. Thus, after the paint
			// that timer will be triggered and the events will be executed.
			if(HasEventHandler())
			{
				sdr::event::TimerEventHandler& rEventHandler = GetEventHandler();

				if(!rEventHandler.IsEmpty())
				{
					rEventHandler.Restart();
				}
			}
		}

		// Process the whole displaying. Only use given DsiplayInfo, do not access other
		// OutputDevices then the given ones.
		void ObjectContactOfPageView::DoProcessDisplay(DisplayInfo& rDisplayInfo)
		{
			// visualize entered group when that feature is switched on and it's not
			// a print output. #i29129# No ghosted display for printing.
			const bool bVisualizeEnteredGroup(DoVisualizeEnteredGroup() && !isOutputToPrinter());

			// Visualize entered groups: Set to ghosted as default
			// start. Do this only for the DrawPage, not for MasterPages
			if(bVisualizeEnteredGroup)
			{
				rDisplayInfo.SetGhostedDrawMode();
			}

			// #114359# save old and set clip region
			OutputDevice* pOutDev = TryToGetOutputDevice();
			OSL_ENSURE(0 != pOutDev, "ObjectContactOfPageView without OutDev, someone has overloaded TryToGetOutputDevice wrong (!)");
			sal_Bool bClipRegionPushed(sal_False);
			const Region& rRedrawArea(rDisplayInfo.GetRedrawArea());

			if(!rRedrawArea.IsEmpty())
			{
				bClipRegionPushed = sal_True;
				pOutDev->Push(PUSH_CLIPREGION);
				pOutDev->IntersectClipRegion(rRedrawArea);
			}

			// Get start node and process DrawPage contents
			const ViewObjectContact& rDrawPageVOContact = GetSdrPage().GetViewContact().GetViewObjectContact(*this);

			// update current ViewInformation2D at the ObjectContact
			const double fCurrentTime(getPrimitiveAnimator().GetTime());
			OutputDevice& rTargetOutDev = GetPageWindow().GetPaintWindow().GetTargetOutputDevice();
            basegfx::B2DRange aViewRange;

			// create ViewRange
            if(isOutputToRecordingMetaFile())
            {
                if(isOutputToPDFFile() || isOutputToPrinter())
                {
                    // #i98402# if it's a PDF export, set the ClipRegion as ViewRange. This is
                    // mainly because SW does not use DrawingLayer Page-Oriented and if not doing this,
                    // all existing objects will be collected as primitives and processed.
                    // OD 2009-03-05 #i99876# perform the same also for SW on printing.
                    const Rectangle aLogicClipRectangle(rDisplayInfo.GetRedrawArea().GetBoundRect());

                    aViewRange = basegfx::B2DRange(
                        aLogicClipRectangle.Left(), aLogicClipRectangle.Top(), 
						aLogicClipRectangle.Right(), aLogicClipRectangle.Bottom());
                }
            }
            else
			{
				// use visible pixels, but transform to world coordinates
				aViewRange = rTargetOutDev.GetDiscreteRange();

				// if a clip region is set, use it
				if(!rDisplayInfo.GetRedrawArea().IsEmpty())
				{
					// get logic clip range and create discrete one
					const Rectangle aLogicClipRectangle(rDisplayInfo.GetRedrawArea().GetBoundRect());
					basegfx::B2DRange aLogicClipRange(
						aLogicClipRectangle.Left(), aLogicClipRectangle.Top(), 
						aLogicClipRectangle.Right(), aLogicClipRectangle.Bottom());
					basegfx::B2DRange aDiscreteClipRange(rTargetOutDev.GetViewTransformation() * aLogicClipRange);
					
					// align the discrete one to discrete boundaries (pixel bounds). Also
					// expand X and Y max by one due to Rectangle definition source
					aDiscreteClipRange.expand(basegfx::B2DTuple(
						floor(aDiscreteClipRange.getMinX()), 
						floor(aDiscreteClipRange.getMinY())));
					aDiscreteClipRange.expand(basegfx::B2DTuple(
						1.0 + ceil(aDiscreteClipRange.getMaxX()), 
						1.0 + ceil(aDiscreteClipRange.getMaxY())));

					// intersect current ViewRange with ClipRange
					aViewRange.intersect(aDiscreteClipRange);
				}

				// transform to world coordinates
                aViewRange.transform(rTargetOutDev.GetInverseViewTransformation());
			}

			// update local ViewInformation2D
			const drawinglayer::geometry::ViewInformation2D aNewViewInformation2D(
				basegfx::B2DHomMatrix(), 
				rTargetOutDev.GetViewTransformation(), 
				aViewRange, 
				GetXDrawPageForSdrPage(&GetSdrPage()),
				fCurrentTime, 
				uno::Sequence<beans::PropertyValue>());
			updateViewInformation2D(aNewViewInformation2D);
			
			// get whole Primitive2DSequence; this will already make use of updated ViewInformation2D
			// and may use the MapMode from the Target OutDev in the DisplayInfo
			drawinglayer::primitive2d::Primitive2DSequence xPrimitiveSequence(rDrawPageVOContact.getPrimitive2DSequenceHierarchy(rDisplayInfo));

			// if there is something to show, use a primitive processor to render it. There
			// is a choice between VCL and Canvas processors currently. The decision is made in
			// createBaseProcessor2DFromOutputDevice and takes into accout things like the
			// Target is a MetaFile, a VDev or something else. The Canvas renderer is triggered
			// currently using the shown boolean. Canvas is not yet the default.
			if(xPrimitiveSequence.hasElements())
			{
				// prepare OutputDevice (historical stuff, maybe soon removed)
				rDisplayInfo.ClearGhostedDrawMode(); // reset, else the VCL-paint with the processor will not do the right thing
				pOutDev->SetLayoutMode(0); // reset, default is no BiDi/RTL

				// create renderer
                drawinglayer::processor2d::BaseProcessor2D* pProcessor2D = createBaseProcessor2DFromOutputDevice(
                    rTargetOutDev, getViewInformation2D());

				if(pProcessor2D)
				{
					pProcessor2D->process(xPrimitiveSequence);
					delete pProcessor2D;
				}
			}

			// #114359# restore old ClipReghion
			if(bClipRegionPushed)
			{
				pOutDev->Pop();
			}

			// Visualize entered groups: Reset to original DrawMode
			if(bVisualizeEnteredGroup)
			{
				rDisplayInfo.ClearGhostedDrawMode();
			}
		}

		// test if visualizing of entered groups is switched on at all
		bool ObjectContactOfPageView::DoVisualizeEnteredGroup() const
		{
			SdrView& rView = GetPageWindow().GetPageView().GetView();
			return rView.DoVisualizeEnteredGroup();
		}

		// get active group's (the entered group) ViewContact
		const ViewContact* ObjectContactOfPageView::getActiveViewContact() const
		{
			SdrObjList* pActiveGroupList = GetPageWindow().GetPageView().GetCurrentObjectList();

			if(pActiveGroupList)
			{
				SdrPage* pSdrPage = dynamic_cast< SdrPage* >(pActiveGroupList);
				
				if(pSdrPage)
				{
					// It's a Page itself
					return &(pSdrPage->GetViewContact());
				}
				else if(pActiveGroupList->getSdrObjectFromSdrObjList())
				{
					// Group object
					return &(pActiveGroupList->getSdrObjectFromSdrObjList()->GetViewContact());
				}
			}
			else
			{
				// use page of associated SdrView
				return &(GetSdrPage().GetViewContact());
			}

			return 0;
		}

		// Invalidate given rectangle at the window/output which is represented by
		// this ObjectContact.
		void ObjectContactOfPageView::InvalidatePartOfView(const basegfx::B2DRange& rRange) const
		{
			// invalidate at associated PageWindow
            GetPageWindow().InvalidatePageWindow(rRange);
		}

		// Get info if given Rectangle is visible in this view
		bool ObjectContactOfPageView::IsAreaVisible(const basegfx::B2DRange& rRange) const
		{
			// compare with the visible rectangle
			if(rRange.isEmpty())
			{
				// no range -> not visible
				return false;
			}
			else
			{
				const OutputDevice& rTargetOutDev = GetPageWindow().GetPaintWindow().GetTargetOutputDevice();
                const basegfx::B2DRange aLogicViewRange(rTargetOutDev.GetLogicRange());

				if(!aLogicViewRange.isEmpty() && !aLogicViewRange.overlaps(rRange))
				{
					return false;
				}
			}

			// call parent
			return ObjectContact::IsAreaVisible(rRange);
		}

		// Get info about the need to visualize GluePoints
		bool ObjectContactOfPageView::AreGluePointsVisible() const
		{
			return GetPageWindow().GetPageView().GetView().ImpIsGlueVisible();
		}

		// check if text animation is allowed.
		bool ObjectContactOfPageView::IsTextAnimationAllowed() const
		{
			SdrView& rView = GetPageWindow().GetPageView().GetView();
			const SvtAccessibilityOptions& rOpt = rView.getAccessibilityOptions();
			return rOpt.GetIsAllowAnimatedText();
		}

		// check if graphic animation is allowed.
		bool ObjectContactOfPageView::IsGraphicAnimationAllowed() const
		{
			SdrView& rView = GetPageWindow().GetPageView().GetView();
			const SvtAccessibilityOptions& rOpt = rView.getAccessibilityOptions();
			return rOpt.GetIsAllowAnimatedGraphics();
		}

		// check if asynchronious graphis loading is allowed. Default is sal_False.
		bool ObjectContactOfPageView::IsAsynchronGraphicsLoadingAllowed() const
		{
			SdrView& rView = GetPageWindow().GetPageView().GetView();
			return rView.IsSwapAsynchron();
		}
			
		// check if buffering of MasterPages is allowed. Default is sal_False.
		bool ObjectContactOfPageView::IsMasterPageBufferingAllowed() const
		{
			SdrView& rView = GetPageWindow().GetPageView().GetView();
			return rView.IsMasterPagePaintCaching();
		}

		// print?
		bool ObjectContactOfPageView::isOutputToPrinter() const
		{
			return (OUTDEV_PRINTER == mrPageWindow.GetPaintWindow().GetOutputDevice().GetOutDevType());
		}

		// window?
		bool ObjectContactOfPageView::isOutputToWindow() const
		{
			return (OUTDEV_WINDOW == mrPageWindow.GetPaintWindow().GetOutputDevice().GetOutDevType());
		}

		// VirtualDevice?
		bool ObjectContactOfPageView::isOutputToVirtualDevice() const
		{
			return (OUTDEV_VIRDEV == mrPageWindow.GetPaintWindow().GetOutputDevice().GetOutDevType());
		}

		// recording MetaFile?
		bool ObjectContactOfPageView::isOutputToRecordingMetaFile() const
		{
			GDIMetaFile* pMetaFile = mrPageWindow.GetPaintWindow().GetOutputDevice().GetConnectMetaFile();
			return (pMetaFile && pMetaFile->IsRecord() && !pMetaFile->IsPause());
		}

		// pdf export?
		bool ObjectContactOfPageView::isOutputToPDFFile() const
		{
            return (0 != mrPageWindow.GetPaintWindow().GetOutputDevice().GetPDFWriter());
		}

		// gray display mode
		bool ObjectContactOfPageView::isDrawModeGray() const
		{
			const sal_uInt32 nDrawMode(mrPageWindow.GetPaintWindow().GetOutputDevice().GetDrawMode());
			return (nDrawMode == (DRAWMODE_GRAYLINE|DRAWMODE_GRAYFILL|DRAWMODE_BLACKTEXT|DRAWMODE_GRAYBITMAP|DRAWMODE_GRAYGRADIENT));
		}

		// gray display mode
		bool ObjectContactOfPageView::isDrawModeBlackWhite() const
		{
			const sal_uInt32 nDrawMode(mrPageWindow.GetPaintWindow().GetOutputDevice().GetDrawMode());
			return (nDrawMode == (DRAWMODE_BLACKLINE|DRAWMODE_BLACKTEXT|DRAWMODE_WHITEFILL|DRAWMODE_GRAYBITMAP|DRAWMODE_WHITEGRADIENT));
		}

		// high contrast display mode
		bool ObjectContactOfPageView::isDrawModeHighContrast() const
		{
			const sal_uInt32 nDrawMode(mrPageWindow.GetPaintWindow().GetOutputDevice().GetDrawMode());
			return (nDrawMode == (DRAWMODE_SETTINGSLINE|DRAWMODE_SETTINGSFILL|DRAWMODE_SETTINGSTEXT|DRAWMODE_SETTINGSGRADIENT));
		}

        // access to SdrView
		SdrView* ObjectContactOfPageView::TryToGetSdrView() const
        {
            return &(mrPageWindow.GetPageView().GetView());
        }


		// access to OutputDevice
		OutputDevice* ObjectContactOfPageView::TryToGetOutputDevice() const
		{
			SdrPreRenderDevice* pPreRenderDevice = mrPageWindow.GetPaintWindow().GetPreRenderDevice();
			
			if(pPreRenderDevice)
			{
				return &(pPreRenderDevice->GetPreRenderDevice());
			}
			else
			{
				return &(mrPageWindow.GetPaintWindow().GetOutputDevice());
			}
		}

		// set all UNO controls displayed in the view to design/alive mode
        void ObjectContactOfPageView::SetUNOControlsDesignMode( bool _bDesignMode )
        {
			if(!getViewObjectContacts().empty())
	        {
				const ViewObjectContactSet::const_iterator aEnd(getViewObjectContacts().end());
				ViewObjectContactSet::iterator aCurrent(getViewObjectContacts().begin());
            
				for(;aCurrent != aEnd; aCurrent++)
		        {
					const ViewObjectContactOfUnoControl* pUnoObjectVOC = dynamic_cast< const ViewObjectContactOfUnoControl* >(*aCurrent);

					if(pUnoObjectVOC)
					{
						pUnoObjectVOC->setControlDesignMode(_bDesignMode);
					}
				}
			}
        }
	} // end of namespace contact
} // end of namespace sdr

//////////////////////////////////////////////////////////////////////////////
// eof

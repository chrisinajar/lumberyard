/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

// include required headers
#include "MotionExtractionWindow.h"
#include "MotionWindowPlugin.h"
#include "../SceneManager/ActorPropertiesWindow.h"
#include "../../../../EMStudioSDK/Source/EMStudioManager.h"
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QIcon>
#include <QCheckBox>
#include <MysticQt/Source/ButtonGroup.h>
#include <EMotionFX/CommandSystem/Source/MotionCommands.h>
#include <EMotionFX/Source/MotionSystem.h>
#include <EMotionFX/Source/SkeletalMotion.h>
#include <EMotionFX/Source/ActorManager.h>
#include <EMotionFX/Source/MotionManager.h>


namespace EMStudio
{
    // constructor
    MotionExtractionWindow::MotionExtractionWindow(QWidget* parent, MotionWindowPlugin* motionWindowPlugin)
        : QWidget(parent)
    {
        mMotionWindowPlugin                 = motionWindowPlugin;
        mSelectCallback                     = nullptr;
        mUnselectCallback                   = nullptr;
        mClearSelectionCallback             = nullptr;
        mWarningWidget                      = nullptr;
        mMainVerticalLayout                 = nullptr;
        mMotionExtractionNodeSelectionWindow= nullptr;
        mWarningSelectNodeLink              = nullptr;
        mAdjustActorCallback                = nullptr;
        mCaptureHeight                      = nullptr;
        mWarningShowed                      = false;
    }


    // destructor
    MotionExtractionWindow::~MotionExtractionWindow()
    {
        GetCommandManager()->RemoveCommandCallback(mSelectCallback, false);
        GetCommandManager()->RemoveCommandCallback(mUnselectCallback, false);
        GetCommandManager()->RemoveCommandCallback(mClearSelectionCallback, false);
        GetCommandManager()->RemoveCommandCallback(mAdjustActorCallback, false);
        delete mAdjustActorCallback;
        delete mSelectCallback;
        delete mUnselectCallback;
        delete mClearSelectionCallback;
    }


#define MOTIONEXTRACTIONWINDOW_HEIGHT 54

    // Create the flags widget.
    void MotionExtractionWindow::CreateFlagsWidget()
    {
        mFlagsWidget = new QWidget();
        mFlagsWidget->setMinimumHeight(MOTIONEXTRACTIONWINDOW_HEIGHT);
        mFlagsWidget->setMaximumHeight(MOTIONEXTRACTIONWINDOW_HEIGHT);

        mCaptureHeight = new QCheckBox("Capture Height Changes");
        connect(mCaptureHeight, &QCheckBox::clicked, this, &MotionExtractionWindow::OnMotionExtractionFlagsUpdated);

        QVBoxLayout* layout = new QVBoxLayout();
        layout->setAlignment(Qt::AlignTop);
        layout->setMargin(0);
        layout->setSpacing(3);
        layout->addWidget(mCaptureHeight);
        mFlagsWidget->setLayout(layout);

        mMainVerticalLayout->addWidget(mFlagsWidget);
    }


    // create the warning widget
    void MotionExtractionWindow::CreateWarningWidget()
    {
        // create the warning widget
        mWarningWidget = new QWidget();
        mWarningWidget->setMinimumHeight(MOTIONEXTRACTIONWINDOW_HEIGHT);
        mWarningWidget->setMaximumHeight(MOTIONEXTRACTIONWINDOW_HEIGHT);

        QLabel* warningLabel = new QLabel("<qt>No node has been selected yet to enable Motion Extraction.</qt>");
        warningLabel->setWordWrap(true);
        warningLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

        mWarningSelectNodeLink = new MysticQt::LinkWidget("Click here to setup the Motion Extraction node", mWarningWidget);
        connect(mWarningSelectNodeLink, &MysticQt::LinkWidget::clicked, this, &MotionExtractionWindow::OnSelectMotionExtractionNode);

        // create and fill the layout
        QVBoxLayout* layout = new QVBoxLayout();

        layout->setMargin(0);
        layout->setAlignment(Qt::AlignTop);

        layout->addWidget(warningLabel);
        layout->addWidget(mWarningSelectNodeLink);

        mWarningWidget->setLayout(layout);

        // add it to our main layout
        mMainVerticalLayout->addWidget(mWarningWidget);
    }


    // init after the parent dock window has been created
    void MotionExtractionWindow::Init()
    {
        // create and register the command callbacks
        mSelectCallback         = new CommandSelectCallback(false);
        mUnselectCallback       = new CommandUnselectCallback(false);
        mClearSelectionCallback = new CommandClearSelectionCallback(false);
        mAdjustActorCallback    = new CommandAdjustActorCallback(false);
        GetCommandManager()->RegisterCommandCallback("AdjustActor", mAdjustActorCallback);
        GetCommandManager()->RegisterCommandCallback("Select", mSelectCallback);
        GetCommandManager()->RegisterCommandCallback("Unselect", mUnselectCallback);
        GetCommandManager()->RegisterCommandCallback("ClearSelection", mClearSelectionCallback);

        // create the node selection windows
        mMotionExtractionNodeSelectionWindow = new NodeSelectionWindow(this, true);
        connect(mMotionExtractionNodeSelectionWindow->GetNodeHierarchyWidget(), static_cast<void (NodeHierarchyWidget::*)(MCore::Array<SelectionItem>)>(&NodeHierarchyWidget::OnSelectionDone), this, &MotionExtractionWindow::OnMotionExtractionNodeSelected);

        // set some layout for our window
        mMainVerticalLayout = new QVBoxLayout();
        mMainVerticalLayout->setMargin(0);
        mMainVerticalLayout->setSpacing(0);
        setLayout(mMainVerticalLayout);

        // default create the warning widget (this is needed else we're getting a crash when switching layouts as the widget and the flag might be out of sync)
        CreateWarningWidget();
        mWarningShowed = true;

        // update interface
        UpdateInterface();
    }


    void MotionExtractionWindow::UpdateInterface()
    {
        const CommandSystem::SelectionList& selectionList = GetCommandManager()->GetCurrentSelection();

        // Check if there actually is any motion selected.
        const uint32 numSelectedMotions = selectionList.GetNumSelectedMotions();
        const bool isEnabled = (numSelectedMotions != 0);

        EMotionFX::ActorInstance* actorInstance = GetCommandManager()->GetCurrentSelection().GetSingleActorInstance();

        // Get the motion extraction and the trajectory node.
        EMotionFX::Actor*   actor          = nullptr;
        EMotionFX::Node*    extractionNode = nullptr;

        if (mCaptureHeight)
        {
            mCaptureHeight->setEnabled( isEnabled );
        }

        if (actorInstance)
        {
            actor           = actorInstance->GetActor();
            extractionNode  = actor->GetMotionExtractionNode();
        }

        if (extractionNode == nullptr)
        {
            // Check if we already show the warning widget, if yes, do nothing.
            if (mWarningShowed == false)
            {
                CreateWarningWidget();

                if (mFlagsWidget)
                {
                    mFlagsWidget->hide();
                    mFlagsWidget->deleteLater();
                    mFlagsWidget = nullptr;
                    mCaptureHeight = nullptr;
                }
            }

            // Disable the link in case no actor is selected.
            if (actorInstance == nullptr)
            {
                mWarningSelectNodeLink->setEnabled(false);
            }
            else
            {
                mWarningSelectNodeLink->setEnabled(true);
            }

            // Return directly in case we show the warning widget.
            mWarningShowed = true;
            return;
        }
        else
        {
            // Check if we already show the motion extraction flags widget, if yes, do nothing.
            if (mWarningShowed)
            {
                if (mWarningWidget)
                {
                    mWarningWidget->hide();
                    mWarningWidget->deleteLater();
                    mWarningWidget = nullptr;
                }

                CreateFlagsWidget();
            }

            if (mCaptureHeight)
            {
                mCaptureHeight->setEnabled( isEnabled );
            }

            // Figure out if all selected motions use the same settings.
            bool allCaptureHeightEqual = true;
            uint32 numCaptureHeight = 0;
            const uint32 numMotions = selectionList.GetNumSelectedMotions();
            bool curCaptureHeight = false;

            for (uint32 i = 0; i < numMotions; ++i)
            {
                EMotionFX::Motion* curMotion = selectionList.GetMotion(i);
                EMotionFX::Motion* prevMotion = (i>0) ? selectionList.GetMotion(i-1) : nullptr;
                curCaptureHeight = (curMotion->GetMotionExtractionFlags() & EMotionFX::MOTIONEXTRACT_CAPTURE_Z);

                if (curCaptureHeight)
                {
                    numCaptureHeight++;
                }

                if (curMotion && prevMotion)
                {
                    if (curCaptureHeight != (prevMotion->GetMotionExtractionFlags() & EMotionFX::MOTIONEXTRACT_CAPTURE_Z))
                    {
                        allCaptureHeightEqual = false;
                    }
                }
            }

            // Adjust the height capture checkbox, based on the selected motions.
            const bool triState = (numMotions > 1) && !allCaptureHeightEqual;
            mCaptureHeight->setTristate( triState );
            if (numMotions > 1)
            {
                if (!allCaptureHeightEqual)
                {
                    mCaptureHeight->setCheckState( Qt::CheckState::PartiallyChecked );
                }
                else
                {
                    mCaptureHeight->setChecked( curCaptureHeight );
                }
            }
            else
            {
                if (numCaptureHeight > 0)
                {
                    mCaptureHeight->setCheckState( Qt::CheckState::Checked );
                }
                else
                {
                    mCaptureHeight->setCheckState( Qt::CheckState::Unchecked );
                }
            }

            mWarningShowed = false;
        }
    }

    
    // The the currently set motion extraction flags from the interface.
    EMotionFX::EMotionExtractionFlags MotionExtractionWindow::GetMotionExtractionFlags() const
    {
        int flags = 0;
        
        if (mCaptureHeight->checkState() == Qt::CheckState::Checked)
            flags |= EMotionFX::MOTIONEXTRACT_CAPTURE_Z;

        return static_cast<EMotionFX::EMotionExtractionFlags>(flags);
    }
    
    
    // Called when any of the motion extraction flags buttons got pressed.
    void MotionExtractionWindow::OnMotionExtractionFlagsUpdated()
    {
        const CommandSystem::SelectionList& selectionList       = GetCommandManager()->GetCurrentSelection();
        const uint32                        numSelectedMotions  = selectionList.GetNumSelectedMotions();
        EMotionFX::ActorInstance*           actorInstance       = selectionList.GetSingleActorInstance();

        // Check if there is at least one motion selected and exactly one actor instance.
        if (!actorInstance || numSelectedMotions == 0)
        {
            return;
        }

        EMotionFX::Actor* actor = actorInstance->GetActor();

        EMotionFX::Node* motionExtractionNode = actor->GetMotionExtractionNode();
        if (!motionExtractionNode)
        {
            MCore::LogWarning("Motion extraction node not set.");
            return;
        }

        // The the currently set motion extraction flags from the interface.
        EMotionFX::EMotionExtractionFlags extractionFlags = GetMotionExtractionFlags();

        MCore::CommandGroup commandGroup("Adjust motion extraction settings", numSelectedMotions);

        // First of all stop all running motions.
        commandGroup.AddCommandString("StopAllMotionInstances");

        // Iterate through all selected motions.
        AZStd::string command;
        for (uint32 i = 0; i < numSelectedMotions; ++i)
        {
            // Get the current selected motion, check if it is a skeletal motion, skip directly elsewise.
            EMotionFX::Motion* motion = selectionList.GetMotion(i);
            if (motion->GetType() != EMotionFX::SkeletalMotion::TYPE_ID)
            {
                continue;
            }

            // Prepare the command and add it to the command group.
            command = AZStd::string::format("AdjustMotion -motionID %i -motionExtractionFlags %i", motion->GetID(), extractionFlags);
            commandGroup.AddCommandString(command.c_str());
        }

        // In case the command group is not empty, execute it.
        if (commandGroup.GetNumCommands() > 0)
        {
            AZStd::string outResult;
            if (GetCommandManager()->ExecuteCommandGroup(commandGroup, outResult) == false)
            {
                if (outResult.empty() == false)
                {
                    MCore::LogError(outResult.c_str());
                }
            }
        }
    }
    

    // open node selection window so that we can select a motion extraction node
    void MotionExtractionWindow::OnSelectMotionExtractionNode()
    {
        EMotionFX::ActorInstance* actorInstance = GetCommandManager()->GetCurrentSelection().GetSingleActorInstance();
        if (actorInstance == nullptr)
        {
            MCore::LogWarning("Cannot open node selection window. Please select an actor instance first.");
            return;
        }

        mMotionExtractionNodeSelectionWindow->Update(actorInstance->GetID());
        mMotionExtractionNodeSelectionWindow->show();
    }


    void MotionExtractionWindow::OnMotionExtractionNodeSelected(MCore::Array<SelectionItem> selection)
    {
        // get the selected node name
        uint32 actorID;
        AZStd::string nodeName;
        ActorPropertiesWindow::GetNodeName(selection, &nodeName, &actorID);

        // create the command group
        MCore::CommandGroup commandGroup("Adjust motion extraction node");

        // adjust the actor
        commandGroup.AddCommandString(AZStd::string::format("AdjustActor -actorID %i -motionExtractionNodeName \"%s\"", actorID, nodeName.c_str()).c_str());

        // execute the command group
        AZStd::string outResult;
        if (GetCommandManager()->ExecuteCommandGroup(commandGroup, outResult) == false)
        {
            MCore::LogError(outResult.c_str());
        }
    }

    //-----------------------------------------------------------------------------------------
    // command callbacks
    //-----------------------------------------------------------------------------------------

    bool UpdateInterfaceMotionExtractionWindow()
    {
        EMStudioPlugin* plugin = EMStudio::GetPluginManager()->FindActivePlugin(MotionWindowPlugin::CLASS_ID);
        if (plugin == nullptr)
        {
            return false;
        }

        MotionWindowPlugin* motionWindowPlugin = (MotionWindowPlugin*)plugin;
        MotionExtractionWindow* motionExtractionWindow = motionWindowPlugin->GetMotionExtractionWindow();

        // is the plugin visible? only update it if it is visible
        //if (motionExtractionWindow->visibleRegion().isEmpty() == false) // TODO: enable this again. problem is we need some callback when it gets visible again and as this is not related to the window plugin but a stack dialog we first need to add some callback for "OnHeaderButtonPressed" inside the dialog stack...also let's not forget to add it to the plugin's VisibilityChanged callback then!
        motionExtractionWindow->UpdateInterface();

        return true;
    }

    bool MotionExtractionWindow::CommandSelectCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        MCORE_UNUSED(command);
        if (CommandSystem::CheckIfHasActorSelectionParameter(commandLine) == false)
        {
            return true;
        }
        return UpdateInterfaceMotionExtractionWindow();
    }

    bool MotionExtractionWindow::CommandSelectCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        MCORE_UNUSED(command);
        if (CommandSystem::CheckIfHasActorSelectionParameter(commandLine) == false)
        {
            return true;
        }
        return UpdateInterfaceMotionExtractionWindow();
    }

    bool MotionExtractionWindow::CommandUnselectCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        MCORE_UNUSED(command);
        if (CommandSystem::CheckIfHasActorSelectionParameter(commandLine) == false)
        {
            return true;
        }
        return UpdateInterfaceMotionExtractionWindow();
    }

    bool MotionExtractionWindow::CommandUnselectCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        MCORE_UNUSED(command);
        if (CommandSystem::CheckIfHasActorSelectionParameter(commandLine) == false)
        {
            return true;
        }
        return UpdateInterfaceMotionExtractionWindow();
    }

    bool MotionExtractionWindow::CommandClearSelectionCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine) { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); return UpdateInterfaceMotionExtractionWindow(); }
    bool MotionExtractionWindow::CommandClearSelectionCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)    { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); return UpdateInterfaceMotionExtractionWindow(); }
    bool MotionExtractionWindow::CommandAdjustActorCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)    { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); return UpdateInterfaceMotionExtractionWindow(); }
    bool MotionExtractionWindow::CommandAdjustActorCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)       { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); return UpdateInterfaceMotionExtractionWindow(); }
} // namespace EMStudio

#include <EMotionFX/Tools/EMotionStudio/Plugins/StandardPlugins/Source/MotionWindow/MotionExtractionWindow.moc>

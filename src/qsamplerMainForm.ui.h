// qsamplerMainForm.ui.h
//
// ui.h extension file, included from the uic-generated form implementation.
/****************************************************************************
   Copyright (C) 2004, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include <qapplication.h>
#include <qeventloop.h>
#include <qworkspace.h>
#include <qprocess.h>
#include <qmessagebox.h>
#include <qdragobject.h>
#include <qfiledialog.h>
#include <qfileinfo.h>
#include <qfile.h>
#include <qtextstream.h>
#include <qstatusbar.h>
#include <qlabel.h>
#include <qtimer.h>

#include "qsamplerAbout.h"
#include "qsamplerOptions.h"
#include "qsamplerMessages.h"

#include "qsamplerChannelStrip.h"
#include "qsamplerOptionsForm.h"

#include "config.h"

#if !defined(WIN32)
#include <unistd.h>
#endif

// Timer constant stuff.
#define QSAMPLER_TIMER_MSECS    500

// Status bar item indexes
#define QSAMPLER_STATUS_CLIENT  0       // Client connection state.
#define QSAMPLER_STATUS_SERVER  1       // Currenr server address (host:port)
#define QSAMPLER_STATUS_CHANNEL 2       // Active channel caption.
#define QSAMPLER_STATUS_SESSION 3       // Current session modification state.


#if defined(WIN32)
static WSADATA _wsaData;
#endif

//-------------------------------------------------------------------------
// qsamplerMainForm -- Main window form implementation.

// Kind of constructor.
void qsamplerMainForm::init (void)
{
    // Initialize some pointer references.
    m_pOptions = NULL;

    // All child forms are to be created later, not earlier than setup.
    m_pMessages = NULL;

    // We'll start clean.
    m_iUntitled = 0;
    m_iDirtyCount = 0;

    m_pServer = NULL;
    m_pClient = NULL;

    m_iStartDelay = 0;
    m_iTimerDelay = 0;

    m_iTimerSlot = 0;

    // Make it an MDI workspace.
    m_pWorkspace = new QWorkspace(this);
    m_pWorkspace->setScrollBarsEnabled(true);
    // Set the activation connection.
    QObject::connect(m_pWorkspace, SIGNAL(windowActivated(QWidget *)), this, SLOT(stabilizeForm()));
    // Make it shine :-)
    setCentralWidget(m_pWorkspace);

    // Create some statusbar labels...
    QLabel *pLabel;
    // Client status.
    pLabel = new QLabel(tr("Connected"), this);
    pLabel->setAlignment(Qt::AlignLeft);
    pLabel->setMinimumSize(pLabel->sizeHint());
    m_status[QSAMPLER_STATUS_CLIENT] = pLabel;
    statusBar()->addWidget(pLabel);
    // Server address.
    pLabel = new QLabel(this);
    pLabel->setAlignment(Qt::AlignLeft);
    m_status[QSAMPLER_STATUS_SERVER] = pLabel;
    statusBar()->addWidget(pLabel, 1);
    // Channel title.
    pLabel = new QLabel(this);
    pLabel->setAlignment(Qt::AlignLeft);
    m_status[QSAMPLER_STATUS_CHANNEL] = pLabel;
    statusBar()->addWidget(pLabel, 2);
    // Session modification status.
    pLabel = new QLabel(tr("MOD"), this);
    pLabel->setAlignment(Qt::AlignHCenter);
    pLabel->setMinimumSize(pLabel->sizeHint());
    m_status[QSAMPLER_STATUS_SESSION] = pLabel;
    statusBar()->addWidget(pLabel);

#if defined(WIN32)
    WSAStartup(MAKEWORD(1, 1), &_wsaData);
#endif
}


// Kind of destructor.
void qsamplerMainForm::destroy (void)
{
    // Stop client and/or server, if not already...
    stopServer();

    // Delete status item labels one by one.
    if (m_status[QSAMPLER_STATUS_CLIENT])
        delete m_status[QSAMPLER_STATUS_CLIENT];
    if (m_status[QSAMPLER_STATUS_SERVER])
        delete m_status[QSAMPLER_STATUS_SERVER];
    if (m_status[QSAMPLER_STATUS_CHANNEL])
        delete m_status[QSAMPLER_STATUS_CHANNEL];
    if (m_status[QSAMPLER_STATUS_SESSION])
        delete m_status[QSAMPLER_STATUS_SESSION];

    // Finally drop any widgets around...
    if (m_pMessages)
        delete m_pMessages;
    if (m_pWorkspace)
        delete m_pWorkspace;

#if defined(WIN32)
    WSACleanup();
#endif
}


// Make and set a proper setup options step.
void qsamplerMainForm::setup ( qsamplerOptions *pOptions )
{
    // We got options?
    m_pOptions = pOptions;

    // Some child forms are to be created right now.
    m_pMessages = new qsamplerMessages(this);
    // Set message defaults...
    updateMessagesFont();
    updateMessagesLimit();
    updateMessagesCapture();
    // Set the visibility signal.
    QObject::connect(m_pMessages, SIGNAL(visibilityChanged(bool)), this, SLOT(stabilizeForm()));

    // Initial decorations toggle state.
    viewMenubarAction->setOn(m_pOptions->bMenubar);
    viewToolbarAction->setOn(m_pOptions->bToolbar);
    viewStatusbarAction->setOn(m_pOptions->bStatusbar);
    channelsAutoArrangeAction->setOn(m_pOptions->bAutoArrange);

    // Initial decorations visibility state.
    viewMenubar(m_pOptions->bMenubar);
    viewToolbar(m_pOptions->bToolbar);
    viewStatusbar(m_pOptions->bStatusbar);

    // Restore whole dock windows state.
    QString sDockables = m_pOptions->settings().readEntry("/Layout/DockWindows" , QString::null);
    if (sDockables.isEmpty()) {
        // Message window is forced to dock on the bottom.
        moveDockWindow(m_pMessages, Qt::DockBottom);
    } else {
        // Make it as the last time.
        QTextIStream istr(&sDockables);
        istr >> *this;
    }
    // Try to restore old window positioning.
    m_pOptions->loadWidgetGeometry(this);

    // Final startup stabilization...
    stabilizeForm();

    // Make it ready :-)
    statusBar()->message(tr("Ready"), 3000);

    // We'll try to start immediately...
    startSchedule(0);

    // Register the first timer slot.
    QTimer::singleShot(QSAMPLER_TIMER_MSECS, this, SLOT(timerSlot()));
}


// Window close event handlers.
bool qsamplerMainForm::queryClose (void)
{
    bool bQueryClose = closeSession(false);

    // Try to save current general state...
    if (m_pOptions) {
        // Some windows default fonts is here on demand too.
        if (bQueryClose && m_pMessages)
            m_pOptions->sMessagesFont = m_pMessages->messagesFont().toString();
        // Try to save current positioning.
        if (bQueryClose) {
            // Save decorations state.
            m_pOptions->bMenubar = MenuBar->isVisible();
            m_pOptions->bToolbar = (fileToolbar->isVisible() || editToolbar->isVisible() || channelsToolbar->isVisible());
            m_pOptions->bStatusbar = statusBar()->isVisible();
            // Save the dock windows state.
            QString sDockables;
            QTextOStream ostr(&sDockables);
            ostr << *this;
            m_pOptions->settings().writeEntry("/Layout/DockWindows", sDockables);
            // And the main windows state.
            m_pOptions->saveWidgetGeometry(this);
        }
    }

    return bQueryClose;
}


void qsamplerMainForm::closeEvent ( QCloseEvent *pCloseEvent )
{
    if (queryClose())
        pCloseEvent->accept();
    else
        pCloseEvent->ignore();
}


void qsamplerMainForm::dragEnterEvent ( QDragEnterEvent* pDragEnterEvent )
{
    bool bAccept = false;

    if (QTextDrag::canDecode(pDragEnterEvent)) {
        QString sUrl;
        if (QTextDrag::decode(pDragEnterEvent, sUrl) && m_pClient)
            bAccept = QFileInfo(QUrl(sUrl).path()).exists();
    }

    pDragEnterEvent->accept(bAccept);
}


void qsamplerMainForm::dropEvent ( QDropEvent* pDropEvent )
{
    if (QTextDrag::canDecode(pDropEvent)) {
        QString sUrl;
        if (QTextDrag::decode(pDropEvent, sUrl) && closeSession(false))
            loadSessionFile(QUrl(sUrl).path());
    }
}


//-------------------------------------------------------------------------
// qsamplerMainForm -- Brainless public property accessors.

// The global options settings property.
qsamplerOptions *qsamplerMainForm::options (void)
{
    return m_pOptions;
}

// The LSCP client descriptor property.
lscp_client_t *qsamplerMainForm::client (void)
{
    return m_pClient;
}


//-------------------------------------------------------------------------
// qsamplerMainForm -- Session file stuff.

// Format the displayable session filename.
QString qsamplerMainForm::sessionName ( bool bFilename )
{
    QString sFilename = m_sFilename;
    if (sFilename.isEmpty())
        sFilename = tr("Untitled") + QString::number(m_iUntitled);
    else if (!bFilename)
        sFilename = QFileInfo(sFilename).fileName();
    return sFilename;
}


// Create a new session file from scratch.
bool qsamplerMainForm::newSession (void)
{
    // Check if we can do it.
    if (!resetSession())
        return false;

    // Ok increment untitled count.
    m_iUntitled++;

    // Stabilize form.
    m_sFilename = QString::null;
    m_iDirtyCount = 0;
    stabilizeForm();

    return true;
}


// Open an existing sampler session.
bool qsamplerMainForm::openSession (void)
{
    if (m_pOptions == NULL)
        return false;

    // Ask for the filename to open...
    QString sFilename = QFileDialog::getOpenFileName(
            m_pOptions->sSessionDir,                // Start here.
            tr("LSCP Session files") + " (*.lscp)", // Filter (LSCP files)
            this, 0,                                // Parent and name (none)
            tr("Open Session")                      // Caption.
    );

    // Have we cancelled?
    if (sFilename.isEmpty())
        return false;

    // Check if we're going to discard safely the current one...
    if (!closeSession(false))
        return false;

    // Load it right away.
    return loadSessionFile(sFilename);
}


// Save current sampler session with another name.
bool qsamplerMainForm::saveSession ( bool bPrompt )
{
    if (m_pOptions == NULL)
        return false;

    QString sFilename = m_sFilename;

    // Ask for the file to save, if there's none...
    if (bPrompt || sFilename.isEmpty()) {
        // If none is given, assume default directory.
        if (sFilename.isEmpty())
            sFilename = m_pOptions->sSessionDir;
        // Prompt the guy...
        sFilename = QFileDialog::getSaveFileName(
                sFilename,                              // Start here.
                tr("LSCP Session files") + " (*.lscp)", // Filter (LSCP files)
                this, 0,                                // Parent and name (none)
                tr("Save Session")                      // Caption.
        );
        // Have we cancelled it?
        if (sFilename.isEmpty())
            return false;
        // Enforce .lscp extension...
        if (QFileInfo(sFilename).extension().isEmpty())
            sFilename += ".lscp";
        // Check if already exists...
        if (sFilename != m_sFilename && QFileInfo(sFilename).exists()) {
            if (QMessageBox::warning(this, tr("Warning"),
                tr("The file already exists:\n\n"
                   "\"%1\"\n\n"
                   "Do you want to replace it?")
                   .arg(sFilename),
                tr("Replace"), tr("Cancel")) > 0)
                return false;
        }
    }

    // Save it right away.
    return saveSessionFile(sFilename);
}


// Close current session.
bool qsamplerMainForm::closeSession ( bool bForce )
{
    bool bClose = true;

    // Are we dirty enough to prompt it?
    if (m_iDirtyCount > 0 && !bForce) {
        switch (QMessageBox::warning(this, tr("Warning"),
            tr("The current session has been changed:\n\n"
            "\"%1\"\n\n"
            "Do you want to save the changes?")
            .arg(sessionName(true)),
            tr("Save"), tr("Discard"), tr("Cancel"))) {
        case 0:     // Save...
            bClose = saveSession(false);
            // Fall thru....
        case 1:     // Discard
            break;
        default:    // Cancel.
            bClose = false;
            break;
        }
    }

    // If we may close it, dot it.
    if (bClose) {
        // Remove all channel strips from sight...
        m_pWorkspace->setUpdatesEnabled(false);
        QWidgetList wlist = m_pWorkspace->windowList();
        for (int iChannel = 0; iChannel < (int) wlist.count(); iChannel++)
            delete (qsamplerChannelStrip *) wlist.at(iChannel);
        m_pWorkspace->setUpdatesEnabled(true);
        // We're now clean, for sure.
        m_iDirtyCount = 0;
    }

    return bClose;
}


// Reload current session.
bool qsamplerMainForm::resetSession (void)
{
    if (m_pClient == NULL)
        return true;

    if (!closeSession(false))
        return false;

    // Now we'll try to create the whole GUI session.
    int iChannels = ::lscp_get_channels(m_pClient);
    if (iChannels < 0) {
        appendMessagesClient("lscp_get_channels");
        appendMessagesError(tr("Could not get current number of channels.\n\nSorry."));
    }

    // Try to catch (re)create each channel.
    m_pWorkspace->setUpdatesEnabled(false);
    for (int iChannelID = 0; iChannelID < iChannels; iChannelID++) {
        createChannel(iChannelID, false);
        QApplication::eventLoop()->processEvents(QEventLoop::ExcludeUserInput);
    }
    m_pWorkspace->setUpdatesEnabled(true);

    return true;
}


// Load a session from specific file path.
bool qsamplerMainForm::loadSessionFile ( const QString& sFilename )
{
    if (m_pClient == NULL)
        return false;

    // Open and read from real file.
    QFile file(sFilename);
    if (!file.open(IO_ReadOnly)) {
        appendMessagesError(tr("Could not open \"%1\" session file.\n\nSorry.").arg(sFilename));
        return false;
    }

    // Read the file.
    int iErrors = 0;
    QTextStream ts(&file);
    while (!ts.atEnd()) {
        // Read the line.
        QString sCommand = ts.readLine().simplifyWhiteSpace();
        // If not empty, nor a comment, call the server...
        if (!sCommand.isEmpty() && sCommand[0] != '#') {
            appendMessagesColor(sCommand, "#996633");
            // Remember that, no matter what,
            // all LSCP commands are CR/LF terminated.
            sCommand += "\r\n";
            if (::lscp_client_query(m_pClient, sCommand.latin1()) != LSCP_OK) {
                appendMessagesClient("lscp_client_query");
                iErrors++;
            }
        }
        // Try to make it snappy :)
        QApplication::eventLoop()->processEvents(QEventLoop::ExcludeUserInput);
    }

    // Ok. we've read it.
    file.close();

    // Have we any errors?
    if (iErrors > 0)
        appendMessagesError(tr("Some setttings could not be loaded\nfrom \"%1\" session file.\n\nSorry.").arg(sFilename));

    // IMPORTANT: We'll refresh every existing channel.
    resetSession();

    // Save as default session directory.
    if (m_pOptions)
        m_pOptions->sSessionDir = QFileInfo(sFilename).dirPath(true);
    // We're not dirty anymore.
    m_iDirtyCount = 0;
    // Stabilize form...
    m_sFilename = sFilename;
    stabilizeForm();
    return true;
}


// Save current session to specific file path.
bool qsamplerMainForm::saveSessionFile ( const QString& sFilename )
{
    // Open and write into real file.
    QFile file(sFilename);
    if (!file.open(IO_WriteOnly | IO_Truncate)) {
        appendMessagesError(tr("Could not open \"%1\" session file.\n\nSorry.").arg(sFilename));
        return false;
    }

    // Write the file.
    int iErrors = 0;
    QTextStream ts(&file);
    ts << "# " << QSAMPLER_TITLE " - " << tr(QSAMPLER_SUBTITLE) << endl;
    ts << "# " << tr("Version")
       << ": " QSAMPLER_VERSION << endl;
    ts << "# " << tr("Build")
       << ": " __DATE__ " " __TIME__ << endl;
    ts << "#"  << endl;
    ts << "# " << tr("File")
       << ": " << QFileInfo(sFilename).fileName() << endl;
    ts << "# " << tr("Date")
       << ": " << QDate::currentDate().toString("MMMM dd yyyy")
       << " "  << QTime::currentTime().toString("hh:mm:ss") << endl;
    ts << "#"  << endl;
    ts << endl;
    QWidgetList wlist = m_pWorkspace->windowList();
    for (int iChannel = 0; iChannel < (int) wlist.count(); iChannel++) {
        qsamplerChannelStrip *pChannel = (qsamplerChannelStrip *) wlist.at(iChannel);
        int iChannelID = pChannel->channelID();
        ts << "# " << pChannel->caption() << endl;
        ts << "ADD CHANNEL" << endl;
        ts << "LOAD ENGINE " << pChannel->engineName() << " " << iChannelID << endl;
        ts << "SET CHANNEL MIDI_INPUT_TYPE " << iChannelID << " " << pChannel->midiDriver() << endl;
        ts << "SET CHANNEL MIDI_INPUT_PORT " << iChannelID << " " << pChannel->midiPort() << endl;
        ts << "SET CHANNEL MIDI_INPUT_CHANNEL " << iChannelID << " " << pChannel->midiChannel() << endl;
        ts << "SET CHANNEL AUDIO_OUTPUT_TYPE " << iChannelID << " " << pChannel->audioDriver() << endl;
        ts << "SET CHANNEL VOLUME " << iChannelID << " " << pChannel->volume() << endl;
        ts << "LOAD INSTRUMENT " << pChannel->instrumentFile() << " " << pChannel->instrumentNr() << " " << iChannelID << endl;
        ts << endl;
        // Try to keep it snappy :)
        QApplication::eventLoop()->processEvents(QEventLoop::ExcludeUserInput);
    }

    // Ok. we've wrote it.
    file.close();

    // Have we any errors?
    if (iErrors > 0)
        appendMessagesError(tr("Some settings could not be saved\nto \"%1\" session file.\n\nSorry.").arg(sFilename));

    // Save as default session directory.
    if (m_pOptions)
        m_pOptions->sSessionDir = QFileInfo(sFilename).dirPath(true);
    // We're not dirty anymore.
    m_iDirtyCount = 0;
    // Stabilize form...
    m_sFilename = sFilename;
    stabilizeForm();
    return true;
}


//-------------------------------------------------------------------------
// qsamplerMainForm -- File Action slots.

// Create a new sampler session.
void qsamplerMainForm::fileNew (void)
{
    // Of course we'll start clean new.
    newSession();
}


// Open an existing sampler session.
void qsamplerMainForm::fileOpen (void)
{
    // Open it right away.
    openSession();
}


// Save current sampler session.
void qsamplerMainForm::fileSave (void)
{
    // Save it right away.
    saveSession(false);
}


// Save current sampler session with another name.
void qsamplerMainForm::fileSaveAs (void)
{
    // Save it right away, maybe with another name.
    saveSession(true);
}


// Restart the client/server instance.
void qsamplerMainForm::fileRestart (void)
{
    if (m_pOptions == NULL)
        return;
        
    bool bRestart = true;
    
    // Ask user whether he/she want's a complete restart...
    // (if we're currently up and running)
    if (bRestart && m_pClient) {
        bRestart = (QMessageBox::warning(this, tr("Warning"),
            tr("New settings will be effective after\n"
               "restarting the client/server connection.\n\n"
               "Please note that this operation may cause\n"
               "temporary MIDI and Audio disruption\n\n"
               "Do you want to restart the connection now?"),
            tr("Restart"), tr("Cancel")) == 0);
    }

    // Are we still for it?
    if (bRestart && closeSession(false)) {
        // Stop server, it will force the client too.
        stopServer();
        // Reschedule a restart...
        startSchedule(m_pOptions->iStartDelay);
    }
}


// Exit application program.
void qsamplerMainForm::fileExit (void)
{
    // Go for close the whole thing.
    close();
}


//-------------------------------------------------------------------------
// qsamplerMainForm -- Edit Action slots.

// Add a new sampler channel.
void qsamplerMainForm::editAddChannel (void)
{
    if (m_pClient == NULL)
        return;

    // Create the new sampler channel.
    int iChannelID = ::lscp_add_channel(m_pClient);
    if (iChannelID < 0) {
        appendMessagesClient("lscp_add_channel");
        appendMessagesError(tr("Could not create the new channel.\n\nSorry."));
        return;
    }

    // Just create the channel strip with given id.
    createChannel(iChannelID, true);

    // We'll be dirty, for sure...
    m_iDirtyCount++;
    // Stabilize form anyway.
    stabilizeForm();
}


// Remove current sampler channel.
void qsamplerMainForm::editRemoveChannel (void)
{
    if (m_pClient == NULL)
        return;

    qsamplerChannelStrip *pChannel = activeChannel();
    if (pChannel == NULL)
        return;

    // Prompt user if he/she's sure about this...
    if (m_pOptions && m_pOptions->bConfirmRemove) {
        if (QMessageBox::warning(this, tr("Warning"),
            tr("About to remove channel:\n\n"
               "%1\n\n"
               "Are you sure?")
               .arg(pChannel->caption()),
            tr("OK"), tr("Cancel")) > 0)
            return;
    }

    // Remove the existing sampler channel.
    if (::lscp_remove_channel(m_pClient, pChannel->channelID()) != LSCP_OK) {
        appendMessagesClient("lscp_remove_channel");
        appendMessagesError(tr("Could not remove channel.\n\nSorry."));
        return;
    }

    // Just delete the channel strip.
    delete pChannel;
    // Do we auto-arrange?
    if (m_pOptions && m_pOptions->bAutoArrange)
        channelsArrange();

    // We'll be dirty, for sure...
    m_iDirtyCount++;
    stabilizeForm();
}


// Setup current sampler channel.
void qsamplerMainForm::editSetupChannel (void)
{
    if (m_pClient == NULL)
        return;

    qsamplerChannelStrip *pChannel = activeChannel();
    if (pChannel == NULL)
        return;

    // Just invoque the channel strip procedure.
    pChannel->channelSetup();
}


// Reset current sampler channel.
void qsamplerMainForm::editResetChannel (void)
{
    if (m_pClient == NULL)
        return;

    qsamplerChannelStrip *pChannel = activeChannel();
    if (pChannel == NULL)
        return;

    // Remove the existing sampler channel.
    if (::lscp_reset_channel(m_pClient, pChannel->channelID()) != LSCP_OK) {
        appendMessagesClient("lscp_reset_channel");
        appendMessagesError(tr("Could not reset channel.\n\nSorry."));
        return;
    }

    // Refresh channel strip info.
    pChannel->updateChannelInfo();
}


//-------------------------------------------------------------------------
// qsamplerMainForm -- View Action slots.

// Show/hide the main program window menubar.
void qsamplerMainForm::viewMenubar ( bool bOn )
{
    if (bOn)
        MenuBar->show();
    else
        MenuBar->hide();
}


// Show/hide the main program window toolbar.
void qsamplerMainForm::viewToolbar ( bool bOn )
{
    if (bOn) {
        fileToolbar->show();
        editToolbar->show();
        channelsToolbar->show();
    } else {
        fileToolbar->hide();
        editToolbar->hide();
        channelsToolbar->hide();
    }
}


// Show/hide the main program window statusbar.
void qsamplerMainForm::viewStatusbar ( bool bOn )
{
    if (bOn)
        statusBar()->show();
    else
        statusBar()->hide();
}


// Show/hide the messages window logger.
void qsamplerMainForm::viewMessages ( bool bOn )
{
    if (bOn)
        m_pMessages->show();
    else
        m_pMessages->hide();
}


// Show options dialog.
void qsamplerMainForm::viewOptions (void)
{
    if (m_pOptions == NULL)
        return;

    qsamplerOptionsForm *pOptionsForm = new qsamplerOptionsForm(this);
    if (pOptionsForm) {
        // Check out some initial nullities(tm)...
        qsamplerChannelStrip *pChannel = activeChannel();
        if (m_pOptions->sDisplayFont.isEmpty() && pChannel)
            m_pOptions->sDisplayFont = pChannel->displayFont().toString();
        if (m_pOptions->sMessagesFont.isEmpty() && m_pMessages)
            m_pOptions->sMessagesFont = m_pMessages->messagesFont().toString();
        // To track down deferred or immediate changes.
        QString sOldServerHost      = m_pOptions->sServerHost;
        int     iOldServerPort      = m_pOptions->iServerPort;
        bool    bOldServerStart     = m_pOptions->bServerStart;
        QString sOldServerCmdLine   = m_pOptions->sServerCmdLine;
        QString sOldDisplayFont     = m_pOptions->sDisplayFont;
        QString sOldMessagesFont    = m_pOptions->sMessagesFont;
        bool    bOldStdoutCapture   = m_pOptions->bStdoutCapture;
        int     bOldMessagesLimit   = m_pOptions->bMessagesLimit;
        int     iOldMessagesLimitLines = m_pOptions->iMessagesLimitLines;
        // Load the current setup settings.
        pOptionsForm->setup(m_pOptions);
        // Show the setup dialog...
        if (pOptionsForm->exec()) {
            // Warn if something will be only effective on next run.
            if (( bOldStdoutCapture && !m_pOptions->bStdoutCapture) ||
                (!bOldStdoutCapture &&  m_pOptions->bStdoutCapture)) {
                QMessageBox::information(this, tr("Information"),
                    tr("Some settings may be only effective\n"
                       "next time you start this program."), tr("OK"));
                updateMessagesCapture();
            }
            // Check wheather something immediate has changed.
            if (sOldDisplayFont != m_pOptions->sDisplayFont)
                updateDisplayFont();
            if (sOldMessagesFont != m_pOptions->sMessagesFont)
                updateMessagesFont();
            if (( bOldMessagesLimit && !m_pOptions->bMessagesLimit) ||
                (!bOldMessagesLimit &&  m_pOptions->bMessagesLimit) ||
                (iOldMessagesLimitLines !=  m_pOptions->iMessagesLimitLines))
                updateMessagesLimit();
            // And now the main thing, whether we'll do client/server recycling?
            if ((sOldServerHost != m_pOptions->sServerHost)      ||
                (iOldServerPort != m_pOptions->iServerPort)      ||
                ( bOldServerStart && !m_pOptions->bServerStart)  ||
                (!bOldServerStart &&  m_pOptions->bServerStart)  ||
                (sOldServerCmdLine != m_pOptions->sServerCmdLine && m_pOptions->bServerStart))
                fileRestart();
        }
        // Done.
        delete pOptionsForm;
    }

    // This makes it.
    stabilizeForm();
}


//-------------------------------------------------------------------------
// qsamplerMainForm -- Channels action slots.

// Arrange channel strips.
void qsamplerMainForm::channelsArrange (void)
{
    // Full width vertical tiling
    QWidgetList wlist = m_pWorkspace->windowList();
    if (wlist.isEmpty())
        return;

    m_pWorkspace->setUpdatesEnabled(false);

    int y = 0;
    for (int iChannel = 0; iChannel < (int) wlist.count(); iChannel++) {
        qsamplerChannelStrip *pChannel = (qsamplerChannelStrip *) wlist.at(iChannel);
    /*  if (pChannel->testWState(WState_Maximized | WState_Minimized)) {
            // Prevent flicker...
            pChannel->hide();
            pChannel->showNormal();
        }   */
        pChannel->adjustSize();
        int iWidth  = m_pWorkspace->width();
        if (iWidth < pChannel->width())
            iWidth = pChannel->width();
    //  int iHeight = pChannel->height() + pChannel->parentWidget()->baseSize().height();
        int iHeight = pChannel->parentWidget()->frameGeometry().height();
        pChannel->parentWidget()->setGeometry(0, y, iWidth, iHeight);
        y += iHeight;
    }

    m_pWorkspace->setUpdatesEnabled(true);
}


// Auto-arrange channel strips.
void qsamplerMainForm::channelsAutoArrange ( bool bOn )
{
    if (m_pOptions == NULL)
        return;

    // Toggle the auto-arrange flag.
    m_pOptions->bAutoArrange = bOn;

    // If on, update whole workspace...
    if (m_pOptions->bAutoArrange)
        channelsArrange();
}


//-------------------------------------------------------------------------
// qsamplerMainForm -- Help Action slots.

// Show information about the Qt toolkit.
void qsamplerMainForm::helpAboutQt (void)
{
    QMessageBox::aboutQt(this);
}


// Show information about application program.
void qsamplerMainForm::helpAbout (void)
{
    // Stuff the about box text...
    QString sText = "<p>\n";
    sText += "<b>" QSAMPLER_TITLE " - " + tr(QSAMPLER_SUBTITLE) + "</b><br />\n";
    sText += "<br />\n";
    sText += tr("Version") + ": <b>" QSAMPLER_VERSION "</b><br />\n";
    sText += "<small>" + tr("Build") + ": " __DATE__ " " __TIME__ "</small><br />\n";
#ifdef CONFIG_DEBUG
    sText += "<small><font color=\"red\">";
    sText += tr("Debugging option enabled.");
    sText += "</font></small><br />";
#endif
    sText += "<br />\n";
    sText += tr("Using") + ": ";
    sText += ::lscp_client_package();
    sText += " ";
    sText += ::lscp_client_version();
    sText += "<br />\n";
    sText += "<br />\n";
    sText += tr("Website") + ": <a href=\"" QSAMPLER_WEBSITE "\">" QSAMPLER_WEBSITE "</a><br />\n";
    sText += "<br />\n";
    sText += "<small>";
    sText += QSAMPLER_COPYRIGHT "<br />\n";
    sText += "<br />\n";
    sText += tr("This program is free software; you can redistribute it and/or modify it") + "<br />\n";
    sText += tr("under the terms of the GNU General Public License version 2 or later.");
    sText += "</small>";
    sText += "</p>\n";

    QMessageBox::about(this, tr("About") + " " QSAMPLER_TITLE, sText);
}

//-------------------------------------------------------------------------
// qsamplerMainForm -- Main window stabilization.

void qsamplerMainForm::stabilizeForm (void)
{
    // Update the main application caption...
    QString sSessioName = sessionName(m_pOptions && m_pOptions->bCompletePath);
    if (m_iDirtyCount > 0)
        sSessioName += '*';
    setCaption(tr(QSAMPLER_TITLE " - [%1]").arg(sSessioName));

    // Update the main menu state...
    qsamplerChannelStrip *pChannel = activeChannel();
    bool bHasClient  = (m_pClient != NULL);
    bool bHasChannel = (bHasClient && pChannel != NULL);
    fileNewAction->setEnabled(bHasClient);
    fileOpenAction->setEnabled(bHasClient);
    fileSaveAction->setEnabled(bHasClient && m_iDirtyCount > 0);
    fileSaveAsAction->setEnabled(bHasClient);
    editAddChannelAction->setEnabled(bHasClient);
    editRemoveChannelAction->setEnabled(bHasChannel);
    editSetupChannelAction->setEnabled(bHasChannel);
    editResetChannelAction->setEnabled(bHasChannel);
    channelsArrangeAction->setEnabled(bHasChannel);
    viewMessagesAction->setOn(m_pMessages && m_pMessages->isVisible());

    // Client status...
    if (bHasClient)
        m_status[QSAMPLER_STATUS_CLIENT]->setText(tr("Connected"));
    else
        m_status[QSAMPLER_STATUS_CLIENT]->clear();
    // Server status...
    if (m_pOptions)
        m_status[QSAMPLER_STATUS_SERVER]->setText(m_pOptions->sServerHost + ":" + QString::number(m_pOptions->iServerPort));
    else
        m_status[QSAMPLER_STATUS_SERVER]->clear();
    // Channel status...
    if (bHasChannel)
        m_status[QSAMPLER_STATUS_CHANNEL]->setText(pChannel->caption());
    else
        m_status[QSAMPLER_STATUS_CHANNEL]->clear();
    // Session status...
    if (m_iDirtyCount > 0)
        m_status[QSAMPLER_STATUS_SESSION]->setText(tr("MOD"));
    else
        m_status[QSAMPLER_STATUS_SESSION]->clear();

    // Always make the latest message visible.
    if (m_pMessages)
        m_pMessages->scrollToBottom();
}


// Channel change receiver slot.
void qsamplerMainForm::channelChanged( qsamplerChannelStrip * )
{
    // Just mark the dirty form.
    m_iDirtyCount++;
    // and update the form status...
    stabilizeForm();
}


// Force update of the channels display font.
void qsamplerMainForm::updateDisplayFont (void)
{
    if (m_pOptions == NULL)
        return;

    // Check if display font is legal.
    if (m_pOptions->sDisplayFont.isEmpty())
        return;
    // Realize it.
    QFont font;
    if (!font.fromString(m_pOptions->sDisplayFont))
        return;

    // Full channel list update...
    QWidgetList wlist = m_pWorkspace->windowList();
    if (wlist.isEmpty())
        return;

    m_pWorkspace->setUpdatesEnabled(false);
    for (int iChannel = 0; iChannel < (int) wlist.count(); iChannel++) {
        qsamplerChannelStrip *pChannel = (qsamplerChannelStrip *) wlist.at(iChannel);
        pChannel->setDisplayFont(font);
    }
    m_pWorkspace->setUpdatesEnabled(true);
}


//-------------------------------------------------------------------------
// qsamplerMainForm -- Messages window form handlers.

// Messages output methods.
void qsamplerMainForm::appendMessages( const QString& s )
{
    if (m_pMessages)
        m_pMessages->appendMessages(s);

    statusBar()->message(s, 3000);
}

void qsamplerMainForm::appendMessagesColor( const QString& s, const QString& c )
{
    if (m_pMessages)
        m_pMessages->appendMessagesColor(s, c);

    statusBar()->message(s, 3000);
}

void qsamplerMainForm::appendMessagesText( const QString& s )
{
    if (m_pMessages)
        m_pMessages->appendMessagesText(s);
}

void qsamplerMainForm::appendMessagesError( const QString& s )
{
    if (m_pMessages)
        m_pMessages->show();

    appendMessagesColor(s.simplifyWhiteSpace(), "#ff0000");

    QMessageBox::critical(this, tr("Error"), s, tr("Cancel"));
}


// This is a special message format, just for client results.
void qsamplerMainForm::appendMessagesClient( const QString& s )
{
    if (m_pClient == NULL)
        return;

    appendMessagesColor(s + QString(": %1 (errno=%2)")
        .arg(::lscp_client_get_result(m_pClient))
        .arg(::lscp_client_get_errno(m_pClient)), "#996666");
}


// Force update of the messages font.
void qsamplerMainForm::updateMessagesFont (void)
{
    if (m_pOptions == NULL)
        return;

    if (m_pMessages && !m_pOptions->sMessagesFont.isEmpty()) {
        QFont font;
        if (font.fromString(m_pOptions->sMessagesFont))
            m_pMessages->setMessagesFont(font);
    }
}


// Update messages window line limit.
void qsamplerMainForm::updateMessagesLimit (void)
{
    if (m_pOptions == NULL)
        return;

    if (m_pMessages) {
        if (m_pOptions->bMessagesLimit)
            m_pMessages->setMessagesLimit(m_pOptions->iMessagesLimitLines);
        else
            m_pMessages->setMessagesLimit(0);
    }
}


// Enablement of the messages capture feature.
void qsamplerMainForm::updateMessagesCapture (void)
{
    if (m_pOptions == NULL)
        return;

    if (m_pMessages)
        m_pMessages->setCaptureEnabled(m_pOptions->bStdoutCapture);
}


//-------------------------------------------------------------------------
// qsamplerMainForm -- MDI channel strip management.

// The channel strip creation executive.
void qsamplerMainForm::createChannel ( int iChannelID, bool bPrompt )
{
    if (m_pClient == NULL)
        return;

    // Prepare for auto-arrange?
    qsamplerChannelStrip *pChannel = NULL;
    int y = 0;
    if (m_pOptions && m_pOptions->bAutoArrange) {
        QWidgetList wlist = m_pWorkspace->windowList();
        for (int iChannel = 0; iChannel < (int) wlist.count(); iChannel++) {
            pChannel = (qsamplerChannelStrip *) wlist.at(iChannel);
        //  y += pChannel->height() + pChannel->parentWidget()->baseSize().height();
            y += pChannel->parentWidget()->frameGeometry().height();
        }
    }

    // Add a new channel itema...
    WFlags wflags = Qt::WStyle_Customize | Qt::WStyle_Tool | Qt::WStyle_Title | Qt::WStyle_NoBorder;
    pChannel = new qsamplerChannelStrip(m_pWorkspace, 0, wflags);
    pChannel->setup(this, iChannelID);
    // We'll need a display font.
    QFont font;
    if (m_pOptions && font.fromString(m_pOptions->sDisplayFont))
        pChannel->setDisplayFont(font);
    // Track channel setup changes.
    QObject::connect(pChannel, SIGNAL(channelChanged(qsamplerChannelStrip *)), this, SLOT(channelChanged(qsamplerChannelStrip *)));
    // Before we show it up, may be we'll
    // better ask for some initial values?
    if (bPrompt)
        pChannel->channelSetup();	
    // Now we show up us to the world.
    pChannel->show();
    // Only then, we'll auto-arrange...
    if (m_pOptions && m_pOptions->bAutoArrange) {
        int iWidth  = m_pWorkspace->width();
    //  int iHeight = pChannel->height() + pChannel->parentWidget()->baseSize().height();
        int iHeight = pChannel->parentWidget()->frameGeometry().height();
        pChannel->parentWidget()->setGeometry(0, y, iWidth, iHeight);
    }
}


// Retrieve the active channel strip.
qsamplerChannelStrip *qsamplerMainForm::activeChannel (void)
{
    return (qsamplerChannelStrip *) m_pWorkspace->activeWindow();
}


// Retrieve a channel strip by index.
qsamplerChannelStrip *qsamplerMainForm::channelAt ( int iChannel )
{
    QWidgetList wlist = m_pWorkspace->windowList();
    if (wlist.isEmpty())
        return 0;

    return (qsamplerChannelStrip *) wlist.at(iChannel);
}


// Construct the windows menu.
void qsamplerMainForm::channelsMenuAboutToShow (void)
{
    channelsMenu->clear();
    channelsArrangeAction->addTo(channelsMenu);
    channelsAutoArrangeAction->addTo(channelsMenu);

    QWidgetList wlist = m_pWorkspace->windowList();
    if (!wlist.isEmpty()) {
        channelsMenu->insertSeparator();
        for (int iChannel = 0; iChannel < (int) wlist.count(); iChannel++) {
            qsamplerChannelStrip *pChannel = (qsamplerChannelStrip *) wlist.at(iChannel);
            int iItemID = channelsMenu->insertItem(pChannel->caption(), this, SLOT(channelsMenuActivated(int)));
            channelsMenu->setItemParameter(iItemID, iChannel);
            channelsMenu->setItemChecked(iItemID, activeChannel() == pChannel);
        }
    }
}


// Windows menu activation slot
void qsamplerMainForm::channelsMenuActivated ( int iChannel )
{
    qsamplerChannelStrip *pChannel = channelAt(iChannel);
    if (pChannel)
        pChannel->showNormal();
    pChannel->setFocus();
}


//-------------------------------------------------------------------------
// qsamplerMainForm -- Timer stuff.

// Set the pseudo-timer delay schedule.
void qsamplerMainForm::startSchedule ( int iStartDelay )
{
    m_iStartDelay  = 1 + (iStartDelay * 1000);
    m_iTimerDelay  = 0;
}

// Suspend the pseudo-timer delay schedule.
void qsamplerMainForm::stopSchedule (void)
{
    m_iStartDelay  = 0;
    m_iTimerDelay  = 0;
}

// Timer slot funtion.
void qsamplerMainForm::timerSlot (void)
{
    if (m_pOptions == NULL)
        return;

    // Is it the first shot on server start after a few delay?
    if (m_iTimerDelay < m_iStartDelay) {
        m_iTimerDelay += QSAMPLER_TIMER_MSECS;
        if (m_iTimerDelay >= m_iStartDelay) {
            // If we cannot start it now, maybe a lil'mo'later ;)
            if (!startClient()) {
                m_iStartDelay += m_iTimerDelay;
                m_iTimerDelay  = 0;
            }
        }
    }
    
	// Refresh each channel usage, on each period...
    if (m_pClient && m_pOptions->bAutoRefresh) {
        m_iTimerSlot += QSAMPLER_TIMER_MSECS;
        if (m_iTimerSlot >= m_pOptions->iAutoRefreshTime)  {
            m_iTimerSlot = 0;
            QWidgetList wlist = m_pWorkspace->windowList();
            for (int iChannel = 0; iChannel < (int) wlist.count(); iChannel++) {
                qsamplerChannelStrip *pChannel = (qsamplerChannelStrip *) wlist.at(iChannel);
                if (pChannel->isVisible())
                    pChannel->updateChannelUsage();
            }
        }
    }

    // Register the next timer slot.
    QTimer::singleShot(QSAMPLER_TIMER_MSECS, this, SLOT(timerSlot()));
}


//-------------------------------------------------------------------------
// qsamplerMainForm -- Server stuff.

// Start linuxsampler server...
void qsamplerMainForm::startServer (void)
{
    if (m_pOptions == NULL)
        return;

    // Aren't already a client, are we?
    if (!m_pOptions->bServerStart || m_pClient)
        return;

    // Is the server process instance still here?
    if (m_pServer) {
        switch (QMessageBox::warning(this, tr("Warning"),
            tr("Could not start the LinuxSampler server.\n\n"
               "Maybe it ss already started."),
            tr("Stop"), tr("Kill"), tr("Cancel"))) {
          case 0:
            m_pServer->tryTerminate();
            break;
          case 1:
            m_pServer->kill();
            break;
        }
        return;
    }

    // Reset our timer counters...
    stopSchedule();

    // OK. Let's build the startup process...
    m_pServer = new QProcess(this);

    // Setup stdout/stderr capture...
    if (m_pOptions->bStdoutCapture) {
        m_pServer->setCommunication(QProcess::Stdout | QProcess::Stderr | QProcess::DupStderr);
        QObject::connect(m_pServer, SIGNAL(readyReadStdout()), this, SLOT(readServerStdout()));
        QObject::connect(m_pServer, SIGNAL(readyReadStderr()), this, SLOT(readServerStdout()));
    }
    // The unforgiveable signal communication...
    QObject::connect(m_pServer, SIGNAL(processExited()), this, SLOT(processServerExit()));

    // Build process arguments...
    m_pServer->setArguments(QStringList::split(' ', m_pOptions->sServerCmdLine));

    appendMessages(tr("Server is starting..."));
    appendMessagesColor(m_pOptions->sServerCmdLine, "#990099");

    // Go jack, go...
    if (!m_pServer->start()) {
        appendMessagesError(tr("Could not start server.\n\nSorry."));
        processServerExit();
        return;
    }

    // Show startup results...
    appendMessages(tr("Server was started with PID=%1.").arg((long) m_pServer->processIdentifier()));

    // Reset (yet again) the timer counters,
    // but this time is deferred as the user opted.
    startSchedule(m_pOptions->iStartDelay);
    stabilizeForm();
}


// Stop linuxsampler server...
void qsamplerMainForm::stopServer (void)
{
    // Stop client code.
    stopClient();

    // And try to stop server.
    if (m_pServer) {
        appendMessages(tr("Server is stopping..."));
        if (m_pServer->isRunning()) {
            m_pServer->tryTerminate();
            return;
        }
     }

     // Do final processing anyway.
     processServerExit();
}


// Stdout handler...
void qsamplerMainForm::readServerStdout (void)
{
    if (m_pMessages)
        m_pMessages->appendStdoutBuffer(m_pServer->readStdout());
}


// Linuxsampler server cleanup.
void qsamplerMainForm::processServerExit (void)
{
    // Force client code cleanup.
    stopClient();

    // Flush anything that maybe pending...
    if (m_pMessages)
        m_pMessages->flushStdoutBuffer();

    if (m_pServer) {
        // Force final server shutdown...
        appendMessages(tr("Server was stopped with exit status %1.").arg(m_pServer->exitStatus()));
        if (!m_pServer->normalExit())
            m_pServer->kill();
        // Destroy it.
        delete m_pServer;
        m_pServer = NULL;
    }

    // Again, make status visible stable.
    stabilizeForm();
}


//-------------------------------------------------------------------------
// qsamplerMainForm -- Client stuff.


// The LSCP client callback procedure.
lscp_status_t qsampler_client_callback ( lscp_client_t *pClient, const char *pchBuffer, int cchBuffer, void *pvData )
{
    qsamplerMainForm *pMainForm = (qsamplerMainForm *) pvData;
    if (pMainForm == NULL)
        return LSCP_FAILED;

    char *pszBuffer = (char *) malloc(cchBuffer + 1);
    if (pszBuffer == NULL)
        return LSCP_FAILED;

    memcpy(pszBuffer, pchBuffer, cchBuffer);
    pszBuffer[cchBuffer] = (char) 0;
    pMainForm->appendMessagesColor(pszBuffer, "#996699");
    free(pszBuffer);

    return LSCP_OK;
}


// Start our almighty client...
bool qsamplerMainForm::startClient (void)
{
    // Have it a setup?
    if (m_pOptions == NULL)
        return false;

    // Aren't we already started, are we?
    if (m_pClient)
        return true;

    // Log prepare here.
    appendMessages(tr("Client connecting..."));

    // Create the client handle...
    m_pClient = ::lscp_client_create(m_pOptions->sServerHost.latin1(), m_pOptions->iServerPort, qsampler_client_callback, this);
    if (m_pClient == NULL) {
        // Is this the first try?
        // maybe we need to start a local server...
        if ((m_pServer && m_pServer->isRunning()) || !m_pOptions->bServerStart)
            appendMessagesError(tr("Could not connect to server as client.\n\nSorry."));
        else
            startServer();
        // This is always a failure.
        stabilizeForm();
        return false;
    }

    // We may stop scheduling around.
    stopSchedule();

    // We'll accept drops from now on...
    setAcceptDrops(true);

    // Log success here.
    appendMessages(tr("Client connected."));

    // Is any session pending to be loaded?
    if (!m_pOptions->sSessionFile.isEmpty()) {
        // Just load the prabably startup session...
        if (loadSessionFile(m_pOptions->sSessionFile)) {
            m_pOptions->sSessionFile = QString::null;
            return true;
        }
    }

    // Make a new session
    return newSession();
}


// Stop client...
void qsamplerMainForm::stopClient (void)
{
    if (m_pClient == NULL)
        return;

    // Log prepare here.
    appendMessages(tr("Client disconnecting..."));

    // Clear timer counters...
    stopSchedule();

    // We'll reject drops from now on...
    setAcceptDrops(false);

    // Force any channel strips around.
    closeSession(true);
    
    // Close us as a client...
    lscp_client_destroy(m_pClient);
    m_pClient = NULL;

    // Log final here.
    appendMessages(tr("Client disconnected."));

    // Make visible status.
    stabilizeForm();
}


// end of qsamplerMainForm.ui.h

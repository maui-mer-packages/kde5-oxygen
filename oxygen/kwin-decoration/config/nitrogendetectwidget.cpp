
//////////////////////////////////////////////////////////////////////////////
// nitrogendetectwidget.cpp
// Note: this class is a stripped down version of
// /kdebase/workspace/kwin/kcmkwin/kwinrules/detectwidget.cpp
// Copyright (c) 2004 Lubos Lunak <l.lunak@kde.org>
// -------------------
//
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <QButtonGroup>
#include <QLayout>
#include <QGroupBox>
#include <KLocale>
#include <QMouseEvent>

#include "nitrogendetectwidget.h"
#include "nitrogendetectwidget.moc"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <fixx11h.h>
#include <QX11Info>

namespace Nitrogen
{

  //_________________________________________________________
  DetectWidget::DetectWidget( QWidget* parent ):
    QWidget( parent )
  {

    QVBoxLayout* vboxLayout = new QVBoxLayout();
    vboxLayout->setMargin( 0 );
    setLayout( vboxLayout );

    QGroupBox* box( new QGroupBox( i18n( "Information about Selected Window" ), this ) );
    vboxLayout->addWidget( box );

    // display layout
    QGridLayout *gridLayout = new QGridLayout();
    box->setLayout( gridLayout );

    // class
    gridLayout->addWidget( new QLabel( i18n( "Class: " ), box ), 0, 0, 1, 1, Qt::AlignRight|Qt::AlignVCenter );
    gridLayout->addWidget( windowClass = new QLabel( box ), 0, 1, 1, 1 );

    // title
    gridLayout->addWidget( new QLabel( i18n( "Title: " ), box ), 1, 0, 1, 1, Qt::AlignRight|Qt::AlignVCenter );
    gridLayout->addWidget( windowTitle = new QLabel( box ), 1, 1, 1, 1 );

    box = new QGroupBox( i18n( "Window Property Selection" ), this );
    QButtonGroup* group( new QButtonGroup( this ) );
    box->setLayout( new QVBoxLayout() );
    vboxLayout->addWidget( box );

    QCheckBox* checkbox;
    group->addButton( checkbox = new QCheckBox( i18n( "Use window class (whole application)" ) ) );
    checkboxes.insert( std::make_pair( checkbox, NitrogenException::WindowClassName ) );
    checkbox->setChecked( true );
    box->layout()->addWidget( checkbox );

    group->addButton( checkbox = new QCheckBox( i18n( "Use window title" ) ) );
    checkboxes.insert( std::make_pair( checkbox, NitrogenException::WindowTitle ) );
    box->layout()->addWidget( checkbox );

  }

  //_________________________________________________________
  NitrogenException::Type DetectWidget::exceptionType( void ) const
  {
    for( CheckBoxMap::const_iterator iter = checkboxes.begin(); iter != checkboxes.end(); iter++ )
    { if( iter->first->isChecked() ) return iter->second; }

    assert( false );
    return NitrogenException::WindowClassName;
  }

  //_________________________________________________________
  DetectDialog::DetectDialog( QWidget* parent ):
    KDialog( parent ),
    grabber( 0 )
  {

    // define buttons
    setButtons( Ok|Cancel );
    showButtonSeparator( false );

    // central widget
    setMainWidget( widget = new DetectWidget( this ) );

  }

  //_________________________________________________________
  void DetectDialog::detect(  WId window )
  {
    if( window == 0 ) selectWindow();
    else readWindow( window );
  }

  //_________________________________________________________
  void DetectDialog::readWindow( WId window )
  {

    if( window == 0 )
    {
      emit detectionDone( false );
      return;
    }

    info = KWindowSystem::windowInfo( window, -1U, -1U );
    if( !info.valid())
    {
      emit detectionDone( false );
      return;
    }

    QString wmclass_class = info.windowClassClass();
    QString wmclass_name = info.windowClassName();
    QString title = info.name();

    widget->setWindowClass( wmclass_class + " (" + wmclass_name + ' ' + wmclass_class + ')' );
    widget->setWindowTitle( title );
    emit detectionDone( exec() == KDialog::Accepted );

    return;

  }

  //_________________________________________________________
  void DetectDialog::selectWindow()
  {

    // use a dialog, so that all user input is blocked
    // use WX11BypassWM and moving away so that it's not actually visible
    // grab only mouse, so that keyboard can be used e.g. for switching windows
    grabber = new KDialog( 0, Qt::X11BypassWindowManagerHint );
    grabber->move( -1000, -1000 );
    grabber->setModal( true );
    grabber->show();
    grabber->grabMouse( Qt::CrossCursor );
    grabber->installEventFilter( this );

  }

  //_________________________________________________________
  bool DetectDialog::eventFilter( QObject* o, QEvent* e )
  {
    // check object and event type
    if( o != grabber ) return false;
    if( e->type() != QEvent::MouseButtonRelease ) return false;

    // delete old grabber
    delete grabber;
    grabber = 0;

    // check button
    if( static_cast< QMouseEvent* >( e )->button() != Qt::LeftButton ) return true;

    // read window information
    readWindow( findWindow() );

    return true;
  }

  //_________________________________________________________
  WId DetectDialog::findWindow()
  {

    Window root;
    Window child;
    uint mask;
    int rootX, rootY, x, y;
    Window parent = QX11Info::appRootWindow();
    Atom wm_state = XInternAtom( QX11Info::display(), "WM_STATE", False );

    // why is there a loop of only 10 here
    for( int i = 0; i < 10; ++i )
    {
      XQueryPointer( QX11Info::display(), parent, &root, &child, &rootX, &rootY, &x, &y, &mask );
      if( child == None ) return 0;
      Atom type;
      int format;
      unsigned long nitems, after;
      unsigned char* prop;
      if( XGetWindowProperty(
        QX11Info::display(), child, wm_state, 0, 0, False,
        AnyPropertyType, &type, &format, &nitems, &after, &prop ) == Success )
      {
        if( prop != NULL ) XFree( prop );
        if( type != None ) return child;
      }
      parent = child;
    }

    return 0;

  }

}

#include <Dynaview.h>

// ui
#include <ui_Dynaview.h>

#include <vtkSphereSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>

#include <vtkPolyDataReader.h>
#include <vtkProperty.h>
#include <vtkNrrdReader.h>
#include <vtkImageViewer2.h>
//#include <vtkImagevi
#include <vtkDICOMImageReader.h>

#include <vtkImageActor.h>
#include <vtkImageMapper3D.h>
#include <vtkImageSliceMapper.h>

#include <vtkImagePlaneWidget.h>
#include <vtkImageMapper.h>
#include <vtkImageData.h>
#include <vtkActor2D.h>
#include <vtkImageMapToColors.h>
#include <vtkImageMapToWindowLevelColors.h>
#include <vtkLookupTable.h>
#include <vtkImageSlice.h>
#include <vtkImageResliceMapper.h>
#include <vtkPlaneSource.h>
#include <vtkLineSource.h>
#include <vtkImageShiftScale.h>
#include <vtkImageCanvasSource2D.h>
#include <vtkMatrix4x4.h>
#include <vtkTransform.h>
#include <vtkCamera.h>

#include <vtkMedicalImageReader2.h>

//#include <vtkGDCM

#include <QFile>
#include <QTextStream>

//---------------------------------------------------------------------------
Dynaview::Dynaview()
{
  this->ui_ = new Ui_Dynaview;
  this->ui_->setupUi( this );
  this->setStatusBar( NULL );
  this->ui_->qvtk_widget->GetRenderWindow()->LineSmoothingOn();
}

//---------------------------------------------------------------------------
Dynaview::~Dynaview()
{}

//---------------------------------------------------------------------------
void Dynaview::initialize_vtk()
{
  this->renderer_ = vtkRenderer::New();

  //this->renderer_->SetRenderWindow( this->ui_->qvtk_widget->GetRenderWindow() );

  this->ui_->qvtk_widget->GetRenderWindow()->AddRenderer( this->renderer_ );

//  this->renderer_->SetBackground( .3, .6, .3 ); // Background color green

  this->renderer_->SetAmbient( 1, 1, 1 );

  //Set the color of the sphere
  //actor->GetProperty()->SetColor( r, g, b ); //(R,G,B)
}

//---------------------------------------------------------------------------
int Dynaview::read_files( int argc, char** argv )
{
  this->argc_ = argc;
  this->argv_ = argv;

  this->renderer_->RemoveAllViewProps();

  if ( argc < 13 )
  {
    std::cerr << "Usage: dynaview <endo.vtk> <S.csv> lmx1.csv zmr.csv fluoro1.dcm fluoro2.dcm MX1.csv"
                 " MX2.csv lm_mr.csv MPro.csv lmx2.csv lmx.csv\n";
    return -1;
  }

  /// origin
  if ( this->ui_->origin_checkbox->isChecked() )
  {
    vtkSmartPointer<vtkSphereSource> sphereSource = vtkSmartPointer<vtkSphereSource>::New();
    sphereSource->SetRadius( 10.0 );

    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection( sphereSource->GetOutputPort() );

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper( mapper );
    this->renderer_->AddActor( actor );
  }

  //dynaview.add_vtk_file( argv[1] );

  // S.csv
  //dynaview.add_spheres( argv[2], 1, 0, 0 );

  vtkPoints* sources = Dynaview::read_points( argv[2] );

  if ( this->ui_->sources_checkbox->isChecked() )
  {
    this->add_spheres( sources, 1, 0, 0 );
  }

  // lmx1.csv
  vtkPoints* f1_points = Dynaview::read_points( argv[3] );
  // lmx2.csv
  vtkPoints* f2_points = Dynaview::read_points( argv[11] );
  if ( this->ui_->landmarks_checkbox->isChecked() )
  {
    this->add_spheres( f1_points, 0, 1, 0 );
    this->add_spheres( f2_points, 0, 1, 0 );
    // zmr.csv
    this->add_spheres( argv[4], 0, 1, 0 );
  }

  if ( this->ui_->lines_checkbox->isChecked() )
  {
    this->add_line( sources->GetPoint( 1 ), f1_points->GetPoint( 0 ) );
    this->add_line( sources->GetPoint( 0 ), f2_points->GetPoint( 0 ) );
  }
  // lmx.csv
  //this->add_spheres( argv[12], 0, 0, 1 );

  // flouro1.dcm, MX1.csv
  this->add_dicom( argv[5], argv[7] );

  // flouro2.dcm, MX1.csv
  this->add_dicom( argv[6], argv[8] );

  // original CS landmarks (lm_mr)
  //this->add_spheres( argv[9], 1, 0, 1 );

  if ( this->ui_->atrium_checkbox->isChecked() )
  {
    this->add_vtk_file( argv[1], argv[10] );
  }

  this->ui_->qvtk_widget->GetRenderWindow()->Render();

  return 0;
}

//---------------------------------------------------------------------------
void Dynaview::add_vtk_file( std::string filename, std::string matrix_filename )
{

  vtkSmartPointer<vtkPolyData> input1;

  vtkSmartPointer<vtkPolyDataReader> reader1 = vtkSmartPointer<vtkPolyDataReader>::New();
  reader1->SetFileName( filename.c_str() );
  reader1->Update();
  input1 = reader1->GetOutput();

  vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  mapper->SetInputConnection( reader1->GetOutputPort() );

  vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
  actor->SetMapper( mapper );

  vtkTransform* transform = vtkTransform::New();
  transform->Identity();

  if ( matrix_filename != "" )
  {
    vtkMatrix4x4* matrix = this->read_matrix( matrix_filename );
    transform->Concatenate( matrix );
  }
  //transform->RotateZ( 180 );

  actor->SetUserTransform( transform );

  double opacity = this->ui_->opacity_slider->value() / 100.0;

  actor->GetProperty()->SetOpacity( opacity );

  this->renderer_->AddActor( actor );
}

//---------------------------------------------------------------------------
void Dynaview::add_spheres( std::string filename, double r, double g, double b )
{

  double x, y, z;

  QFile* file = new QFile( QString::fromStdString( filename ) );

  if ( !file->open( QIODevice::ReadOnly ) )
  {
    std::cerr << "Error opening file for reading: " << filename << "\n";
  }

  QTextStream ts( file );

  int count = 0;
  while ( !ts.atEnd() )
  {
    QString str;
    ts >> str;
    if ( str != "" )
    {
      QStringList parts = str.split( "," );

      double x = parts[0].toDouble();
      double y = parts[1].toDouble();
      double z = 0;
      if ( parts.size() == 2 )
      {
        x = parts[0].toDouble();
/*        y = 0;
        z = -parts[1].toDouble();*/
        y = parts[1].toDouble();
        z = 0;
      }
      if ( parts.size() == 3 )
      {
        z = parts[2].toDouble();
      }

      // Create a sphere
      vtkSmartPointer<vtkSphereSource> sphereSource = vtkSmartPointer<vtkSphereSource>::New();
      sphereSource->SetCenter( x, y, z );
      sphereSource->SetRadius( 5.0 );

      vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
      mapper->SetInputConnection( sphereSource->GetOutputPort() );

      vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
      actor->SetMapper( mapper );

      actor->GetProperty()->SetColor( 1, 0, 0 ); //(R,G,B)

      if ( count != 0 )
      {
        actor->GetProperty()->SetColor( r, g, b ); //(R,G,B)
      }

      count++;

      this->renderer_->AddActor( actor );
    }
  }
  file->close();
}

//---------------------------------------------------------------------------
void Dynaview::add_spheres( vtkPoints* points, double r, double g, double b )
{
  for ( int i = 0; i < points->GetNumberOfPoints(); i++ )
  {
    // Create a sphere
    vtkSmartPointer<vtkSphereSource> sphereSource = vtkSmartPointer<vtkSphereSource>::New();
    sphereSource->SetCenter( points->GetPoint( i ) );
    sphereSource->SetRadius( 5.0 );

    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection( sphereSource->GetOutputPort() );

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper( mapper );

    actor->GetProperty()->SetColor( 1, 0, 0 ); //(R,G,B)

    if ( i != 0 )
    {
      actor->GetProperty()->SetColor( r, g, b ); //(R,G,B)
    }

    this->renderer_->AddActor( actor );
  }
}

//---------------------------------------------------------------------------
void Dynaview::add_line( double* p1, double* p2 )
{
  vtkLineSource* line = vtkLineSource::New();
  vtkPolyDataMapper* mapper = vtkPolyDataMapper::New();
  vtkActor* actor = vtkActor::New();

  line->SetPoint1( p1 );
  line->SetPoint2( p2 );
  line->Update();

  mapper->SetInputData( line->GetOutput() );
  mapper->Update();

  actor->SetMapper( mapper );
  actor->GetProperty()->SetLineWidth( 2 );
  this->renderer_->AddActor( actor );
}

//---------------------------------------------------------------------------
void Dynaview::add_dicom( std::string filename, std::string matrix_filename )
{

  vtkMatrix4x4* matrix = this->read_matrix( matrix_filename );

  // Read all the DICOM files in the specified directory.
  vtkSmartPointer<vtkDICOMImageReader> reader = vtkSmartPointer<vtkDICOMImageReader>::New();
  //reader->SetDirectoryName(folder.c_str());
  reader->SetFileName( filename.c_str() );
  reader->Update();
  //reader->geth

  // Pass the original image and the lookup table to a filter to create
  // a color image:
  vtkSmartPointer<vtkImageMapToWindowLevelColors> scalarValuesToColors =
    vtkSmartPointer<vtkImageMapToWindowLevelColors>::New();
  //scalarValuesToColors->SetLookupTable(lookupTable);
  scalarValuesToColors->PassAlphaToOutputOn();
  scalarValuesToColors->SetInputConnection( reader->GetOutputPort() );
  double* range = reader->GetOutput()->GetScalarRange();
  //double *spacing = reader->GetOutput()->GetSpacing();
  int* dims = reader->GetOutput()->GetDimensions();
  std::cerr << "range = " << range[0] << " - " << range[1] << "\n";
  //std::cerr << "spacing = " << spacing[0] << ", " << spacing[1] << "\n";
  std::cerr << "dims = " << dims[0] << ", " << dims[1] << "\n";

  double spacing = 0.308;

  scalarValuesToColors->SetWindow( range[1] - range[0] );
  scalarValuesToColors->SetLevel( 0.5 * ( range[1] + range[0] ) );
  scalarValuesToColors->SetOutputFormatToLuminance();
  scalarValuesToColors->Update();

  range = scalarValuesToColors->GetOutput()->GetScalarRange();

  vtkSmartPointer<vtkImageShiftScale> xShiftScale =
    vtkSmartPointer<vtkImageShiftScale>::New();
  xShiftScale->SetOutputScalarTypeToUnsignedChar();
  xShiftScale->SetScale( 255 / range[1] );
  xShiftScale->SetInputConnection( scalarValuesToColors->GetOutputPort() );
  xShiftScale->Update();

  std::cerr << "range = " << range[0] << " - " << range[1] << "\n";

  vtkTexture* texture = vtkTexture::New();
  texture->SetInputConnection( xShiftScale->GetOutputPort() );
//texture->SetInputConnection(imageSource->GetOutputPort());

  vtkPlaneSource* plane = vtkPlaneSource::New();
  plane->SetCenter( 0, 0, 0 );
  ///plane->SetCenter( 0.0, 0.0, 0.0 );
  //plane->SetNormal( 0.0, 1.0, 0.0 );

  //plane->SetPoint1( 0, 5120, 0 );
  //plane->SetPoint2( 5120, 0, 0 );

  vtkTransform* transform = vtkTransform::New();

  double dist_x = ( dims[0] / 2 ) * spacing;
  double dist_y = ( dims[1] / 2 ) * spacing;
  std::cerr << "dist_x = " << dist_x << "\n";
  std::cerr << "dist_y = " << dist_y << "\n";

  transform->Concatenate( matrix );

  // center on origin
  //transform->Translate( -dist, 0, -dist );
  transform->Translate( -dist_x, -dist_y, 0 );

  //transform->RotateZ(30);

  //transform->GetMatrix()->Print(std::cerr);

  //plane->SetPoint1( dist, -dist, 0 );
  //plane->SetPoint2( -dist, dist, 0 );

  //plane->SetCenter(-263,-159,-147);

  //plane->SetPoint2( 0, 0, dist * 2 );
  //plane->SetPoint1( dist * 2, 0, 0 );
  plane->SetPoint1( dist_x * 2, 0, 0 );
  plane->SetPoint2( 0, dist_y * 2, 0 );

  //plane->SetOrigin(-dist/2,0,-dist/2);
  ///plane->SetPoint1(100,-100,0);
  //plane->SetPoint2(-100,100,0);

  vtkPolyDataMapper* polymapper = vtkPolyDataMapper::New();
  polymapper->SetInputConnection( plane->GetOutputPort() );

  vtkActor* plane_actor = vtkActor::New();
  plane_actor->SetMapper( polymapper );
  plane_actor->SetTexture( texture );

  //plane_actor->SetUserMatrix( m2 );
  //plane_actor->SetUserMatrix( matrix );

  plane_actor->SetUserTransform( transform );

  //matrix->Print( std::cerr );

  plane_actor->GetProperty()->SetSpecular( 1 );
  plane_actor->GetProperty()->SetAmbient( 1 );
  plane_actor->GetProperty()->SetBackfaceCulling( 0 );

  this->renderer_->AddActor( plane_actor );

  return;
}

//---------------------------------------------------------------------------
vtkMatrix4x4* Dynaview::read_matrix( std::string filename )
{
  QFile* file = new QFile( QString::fromStdString( filename ) );

  if ( !file->open( QIODevice::ReadOnly ) )
  {
    std::cerr << "Error opening file for reading: " << filename << "\n";
    return NULL;
  }

  QTextStream ts( file );

  vtkMatrix4x4* matrix = vtkMatrix4x4::New();

  matrix->Identity();

  int row = 0;
  while ( !ts.atEnd() )
  {
    QString str;
    ts >> str;
    if ( str != "" )
    {
      QStringList parts = str.split( "," );

      matrix->SetElement( row, 0, parts[0].toDouble() );
      matrix->SetElement( row, 1, parts[1].toDouble() );
      matrix->SetElement( row, 2, parts[2].toDouble() );
      matrix->SetElement( row, 3, parts[3].toDouble() );

      row++;
    }
  }
  file->close();

  return matrix;
}

//---------------------------------------------------------------------------
void Dynaview::set_sources( std::string filename )
{}

//---------------------------------------------------------------------------
vtkPoints* Dynaview::read_points( std::string filename )
{
  vtkPoints* points = vtkPoints::New();

  double x, y, z;

  QFile* file = new QFile( QString::fromStdString( filename ) );

  if ( !file->open( QIODevice::ReadOnly ) )
  {
    std::cerr << "Error opening file for reading: " << filename << "\n";
  }

  QTextStream ts( file );

  int count = 0;
  while ( !ts.atEnd() )
  {
    QString str;
    ts >> str;
    if ( str != "" )
    {
      QStringList parts = str.split( "," );

      double x = parts[0].toDouble();
      double y = parts[1].toDouble();
      double z = 0;
      if ( parts.size() == 2 )
      {
        x = parts[0].toDouble();
/*        y = 0;
        z = -parts[1].toDouble();*/
        y = parts[1].toDouble();
        z = 0;
      }
      if ( parts.size() == 3 )
      {
        z = parts[2].toDouble();
      }

      points->InsertNextPoint( x, y, z );
    }
  }
  file->close();
  return points;
}

//---------------------------------------------------------------------------
void Dynaview::on_lines_checkbox_toggled()
{
  this->read_files( this->argc_, this->argv_ );
}

//---------------------------------------------------------------------------
void Dynaview::on_atrium_checkbox_toggled()
{
  this->read_files( this->argc_, this->argv_ );
}

//---------------------------------------------------------------------------
void Dynaview::on_origin_checkbox_toggled()
{
  this->read_files( this->argc_, this->argv_ );
}

//---------------------------------------------------------------------------
void Dynaview::on_sources_checkbox_toggled()
{
  this->read_files( this->argc_, this->argv_ );
}

//---------------------------------------------------------------------------
void Dynaview::on_parallel_checkbox_toggled()
{
  this->renderer_->GetActiveCamera()->SetParallelProjection( this->ui_->parallel_checkbox->isChecked() );
  this->renderer_->ResetCamera();
  this->ui_->qvtk_widget->GetRenderWindow()->Render();
}

//---------------------------------------------------------------------------
void Dynaview::on_landmarks_checkbox_toggled()
{
  this->read_files( this->argc_, this->argv_ );
}

//---------------------------------------------------------------------------
void Dynaview::on_opacity_slider_valueChanged( int value )
{
  this->read_files( this->argc_, this->argv_ );
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

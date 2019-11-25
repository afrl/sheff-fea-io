set(classes
  vtkFEAIOReader)

vtk_module_add_module(FEAIOReaders
  CLASSES ${classes})

paraview_add_server_manager_xmls(
  XMLS  FEAIOReader.xml)

conf_data = configuration_data()
conf_data.set('version', razergenie_version)
conf_data.set('datadir', join_paths(get_option('prefix'), get_option('datadir'), 'razergenie'))

configure_file(input : 'config.h.in',
               output : 'config.h',
               configuration : conf_data)

razergenie_sources = ['main.cpp', 'razergenie.cpp', 'razerimagedownloader.cpp', 'razerdevicewidget.cpp', 'devicelistwidget.cpp', 'util.cpp',
                      'customeditor/customeditor.cpp', 'customeditor/matrixpushbutton.cpp', 'preferences/preferences.cpp']

processed = qt5.preprocess(
  moc_headers : ['razergenie.h', 'razerimagedownloader.h', 'devicelistwidget.h', 'customeditor/customeditor.h', 'preferences/preferences.h'],
  ui_files : '../ui/razergenie.ui'
)

incdir = include_directories('libopenrazer')

razergenie = executable('razergenie',
                        [razergenie_sources, processed],
                        dependencies : qt5_dep,
                        include_directories : incdir,
                        link_with : libopenrazer,
                        install : true)

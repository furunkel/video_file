require 'mkmf'
create_makefile('video_file_ext')

require 'rake/extensiontask'

Rake::ExtensionTask.new('video_file_ext')

require "bundler/gem_tasks"
require "rake/testtask"
require "yard"

Rake::TestTask.new(:test) do |t|
  t.libs << "test"
  t.libs << "lib"
  t.test_files = FileList["test/**/*_test.rb"]
end

require 'rake/extensiontask'
Rake::ExtensionTask.new('video_file_native')

task :default => :test

YARD::Rake::YardocTask.new

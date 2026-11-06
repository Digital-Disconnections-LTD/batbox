#pragma once
#ifndef _WIN32
#include <fcntl.h>
#include <spawn.h>
#include <unistd.h>

namespace batbox::process {
// A child starts with stdin, stdout and stderr only (the rule the Bash Gadget
// follows in shell/Spawn.cpp). Without it a server the child starts keeps
// Batbox's log and other open files after Batbox exits. Call after the file
// actions that set up the child's own descriptors; returns the spawn flags to
// add to the attributes.
inline short keepOnlyStandardDescriptors(posix_spawn_file_actions_t* actions){
#if defined(__APPLE__)
  // Descriptors 0-2 the actions did not replace stay as Batbox has them.
  for(int fd=STDIN_FILENO;fd<=STDERR_FILENO;++fd)
    if(::fcntl(fd,F_GETFD)!=-1)posix_spawn_file_actions_addinherit_np(actions,fd);
  return POSIX_SPAWN_CLOEXEC_DEFAULT;
#elif defined(__GLIBC__)&&(__GLIBC__>2||(__GLIBC__==2&&__GLIBC_MINOR__>=34))
  posix_spawn_file_actions_addclosefrom_np(actions,STDERR_FILENO+1);
  return 0;
#else
#error "Child processes need a way to close inherited descriptors on this platform"
#endif
}
}
#endif

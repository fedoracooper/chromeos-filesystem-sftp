#include <vector>
#include <cstdio>

#include "write_file_command.h"

WriteFileCommand::WriteFileCommand(SftpEventListener *listener,
                                   const int server_sock,
                                   LIBSSH2_SESSION *session,
                                   LIBSSH2_SFTP *sftp_session,
                                   const int request_id,
                                   const std::string &path,
                                   const libssh2_uint64_t offset,
                                   const size_t length,
                                   const pp::VarArrayBuffer &data)
 : AbstractCommand(session, sftp_session, server_sock, listener, request_id),
   path_(path),
   offset_(offset),
   length_(length),
   data_(data)
{
}

WriteFileCommand::~WriteFileCommand()
{
}

void* WriteFileCommand::Start(void *arg)
{
  WriteFileCommand *instance = static_cast<WriteFileCommand*>(arg);
  instance->Execute();
  return NULL;
}

void WriteFileCommand::Execute()
{
  LIBSSH2_SFTP_HANDLE *sftp_handle = NULL;
  try {
    sftp_handle = OpenFile(path_, LIBSSH2_FXF_WRITE, 0);
    WriteFile(sftp_handle, offset_, length_, data_);
    GetListener()->OnWriteFileFinished(GetRequestID());
  } catch(CommunicationException e) {
    std::string msg;
    msg = e.toString();
    GetListener()->OnErrorOccurred(GetRequestID(), msg);
  }
  if (sftp_handle) {
    libssh2_sftp_close(sftp_handle);
  }
  delete this;
}

void WriteFileCommand::WriteFile(LIBSSH2_SFTP_HANDLE *sftp_handle,
                                 const libssh2_uint64_t offset,
                                 const size_t length,
                                 pp::VarArrayBuffer &buffer)
  throw(CommunicationException)
{
  libssh2_sftp_seek64(sftp_handle, offset);
  int rc = -1;
  int w_pos = 0;
  uint32_t data_length = buffer.ByteLength();
  size_t remain = std::min(length, static_cast<size_t>(data_length));
  char* data = static_cast<char*>(buffer.Map());
  do {
    while ((rc = libssh2_sftp_write(sftp_handle, (data + w_pos), remain)) == LIBSSH2_ERROR_EAGAIN) {
      WaitSocket(GetServerSock(), GetSession());
    }
    if (rc < 0) {
      THROW_COMMUNICATION_EXCEPTION("Writing file failed", rc);
    }
    w_pos += rc;
    remain -= rc;
  } while (remain);
}

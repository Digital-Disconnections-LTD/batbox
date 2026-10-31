#include "auth/storage/AuthBinding.hpp"

namespace batbox::auth::storage {
void bindOAuthStorage(OAuthTokenServices& tokens,
    save::Services& saveServices,const Storage& storage){
  tokens.secureStorageRead=storage.read;
  tokens.secureStorageReadAsync=storage.readAsync;
  saveServices.getSecureStorage=[storage](){
    return save::SecureStorage{storage.read,storage.update};
  };
}
}

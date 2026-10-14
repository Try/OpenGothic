#include "openfiledialog.h"

#include <Tempest/Platform>
#include <Tempest/TextCodec>

#include "ui/rootview.h"

#ifdef __WINDOWS__
#include <windows.h>
#include <shobjidl.h>
#endif

#ifdef __WINDOWS__
static std::filesystem::path toPath(IShellItem* itm) {
  PWSTR pszFilePath = nullptr;
  try {
    HRESULT hr = itm->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    // Display the file name to the user.
    if(SUCCEEDED(hr)) {
      std::filesystem::path ret = pszFilePath;
      CoTaskMemFree(pszFilePath);
      return ret;
      }
    }
  catch(...) {
    if(pszFilePath!=nullptr)
      CoTaskMemFree(pszFilePath);
    }

  return std::filesystem::path();
  }

static std::vector<std::filesystem::path> toPath(IShellItemArray* arr) {
  DWORD   count = 0;
  HRESULT hr    = arr->GetCount(&count);
  if(!SUCCEEDED(hr))
    return {};
  std::vector<std::filesystem::path> ret;
  for(DWORD i=0; i<count; ++i) {
    IShellItem *pItem = nullptr;
    if(SUCCEEDED(hr)) {
      try {
        hr = arr->GetItemAt(i,&pItem);
        if(SUCCEEDED(hr)) {
          auto path = toPath(pItem);
          pItem->Release();
          ret.emplace_back(std::move(path));
          }
        }
      catch(...) {
        // skip entry
        }
      }
    }
  return ret;
  }

std::vector<std::filesystem::path>
OpenFileDialog::getOpenFileName(std::string_view /*path*/,
                                const std::initializer_list<std::pair<std::string_view,std::string_view>>& ext,
                                bool multiselect) {
  std::wstring wstr;
  for(auto& e:ext) {
    for(size_t r=0; e.first[r]; ++r)
      wstr.push_back(e.first[r]);
    wstr.push_back('\0');
    for(size_t r=0; e.second[r]; ++r)
      wstr.push_back(e.second[r]);
    wstr.push_back('\0');
    }

  const auto* str = wstr.c_str();
  std::vector<COMDLG_FILTERSPEC> filter{ext.size()};
  for(size_t i=0; i<ext.size(); ++i) {
    auto& e = *(ext.begin()+i);
    filter[i].pszName = str; str += e.first.size()  + 1;
    filter[i].pszSpec = str; str += e.second.size() + 1;
    }

  std::vector<std::filesystem::path> ret;
  IFileOpenDialog *pFileOpen;

  // Create the FileOpenDialog object.
  HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                                IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

  if(SUCCEEDED(hr)) {
    pFileOpen->SetFileTypes(UINT(filter.size()),filter.data());
    // Show the Open dialog box.
    if(multiselect)
      pFileOpen->SetOptions(FOS_NOCHANGEDIR | FOS_ALLOWMULTISELECT); else
      pFileOpen->SetOptions(FOS_NOCHANGEDIR);
    hr = pFileOpen->Show(HWND(RootView::mainWindow()));

    // Get the file name from the dialog box.
    if(SUCCEEDED(hr)) {
      if(multiselect) {
        IShellItemArray* pResults = nullptr;
        hr = pFileOpen->GetResults(&pResults);

        if(SUCCEEDED(hr)) {
          ret = toPath(pResults);
          pResults->Release();
          }
        } else {
        IShellItem *pItem = nullptr;
        hr = pFileOpen->GetResult(&pItem);
        if(SUCCEEDED(hr)) {
          auto p = toPath(pItem);
          pItem->Release();
          if(!p.empty()) {
            try {
              ret.emplace_back(std::move(p));
              }
            catch(...) {
              // return empty array in this case
              }
            }
          }
        }
      }
    pFileOpen->Release();
    }

  return ret;
  }
#else
std::vector<std::filesystem::path>
  OpenFileDialog::getOpenFileName(std::string_view /*path*/,
                                  const std::initializer_list<std::pair<std::string_view,std::string_view>>& ext,
                                  bool multiselect) {
# warning "file dialog not implemented on this OS"
  return {};
  }
#endif

std::filesystem::path OpenFileDialog::getProjOpenFileName(std::string_view path) {
  auto ret = OpenFileDialog::getOpenFileName(path,{{"Mod file (*.ini)", "*.ini"}},false);
  if(ret.size()>0)
    return ret[0];
  return std::filesystem::path();
  }

#include "StdAfx.h"
#include "FileSystemNode.h"
#if defined(CINDER_COCOA)
#include <QuickLook/QuickLook.h>
#endif
#if !defined(CINDER_MSW)
#include <sys/stat.h>
#endif

FileSystemNode::FileSystemNode(std::string const& path, std::shared_ptr<FileSystemNode> const& parent) :
  m_path(boost::filesystem::canonical(path))
{
  if (path == "/") {
    m_path = "/";
  }
  init(parent);
}

FileSystemNode::FileSystemNode(char const* path, std::shared_ptr<FileSystemNode> const& parent) :
  m_path(boost::filesystem::canonical(std::string(path)))
{
  if (std::string(path) == "/") {
    m_path = "/";
  }
  init(parent);
}

FileSystemNode::FileSystemNode(boost::filesystem::path const& path, std::shared_ptr<FileSystemNode> const& parent) :
  m_path(path)
{
  init(parent);
}

FileSystemNode::FileSystemNode(boost::filesystem::path&& path, std::shared_ptr<FileSystemNode> const& parent) :
  m_path(std::move(path))
{
  init(parent);
}

void FileSystemNode::init(std::shared_ptr<FileSystemNode> const& parent)
{
  m_surfaceAttempted = false;
  bool hasParent = (!parent && m_path.has_parent_path());
  std::string name = m_path.filename().string();
#if defined(CINDER_MSW)
  auto path = m_path.wstring();
  const auto pathLength = path.length();
  if (pathLength <= 3) {
    if (pathLength >= 2 && path[1] == L':') {
      if (pathLength == 2) {
        m_path += L'\\';
      } else if (path[2] == L'/') {
        path[2] = L'\\';
        m_path = path;
      }
      WCHAR volumeLabel[MAX_PATH + 1] = L"";
      if (GetVolumeInformationW(m_path.wstring().c_str(), volumeLabel, ARRAYSIZE(volumeLabel),
                                nullptr, nullptr, nullptr, nullptr, 0)) {
        std::wstring volumeName(volumeLabel);
        if (volumeName.empty()) {
          // Generate a description of the device since it doesn't have a label
          UINT type = GetDriveType(m_path.wstring().c_str());
          switch (type) {
            default:
            case DRIVE_UNKNOWN:
              volumeName = L"Unknown Disk"; // Needs to be localied -- FIXME
              break;
            case DRIVE_NO_ROOT_DIR:
              volumeName = L"Invalid Disk"; // Needs to be localied -- FIXME
              break;
            case DRIVE_REMOVABLE:
              volumeName = L"Removable Disk"; // Needs to be localied -- FIXME
              break;
            case DRIVE_FIXED:
              volumeName = L"Local Disk"; // Needs to be localied -- FIXME
              break;
            case DRIVE_REMOTE:
              volumeName = L"Network Disk"; // Needs to be localied -- FIXME
              break;
            case DRIVE_CDROM:
              volumeName = L"CD-ROM Disk"; // Needs to be localied -- FIXME
              break;
            case DRIVE_RAMDISK:
              volumeName = L"RAM Disk"; // Needs to be localied -- FIXME
              break;
          }
        }
        volumeName += L" (" + path.substr(0, 2) + L")";
        name = cinder::toUtf8(volumeName);
      } else {
        name = m_path.string();
      }
    }
    hasParent = false;
  }
#endif
  m_parent = hasParent ? std::shared_ptr<FileSystemNode>(new FileSystemNode(m_path.parent_path())) : parent;
  set_metadata_as("name", name);
  std::string extension = m_path.extension().string();
  if (!extension.empty() && extension[0] == '.') {
    extension.erase(0, 1); // Remove leading '.' in extension (e.g., '.txt' => 'txt')
  }
  set_metadata_as("ext", extension);
  uint64_t size = 0;
  boost::filesystem::file_status file_status = boost::filesystem::status(m_path);
  if (file_status.type() == boost::filesystem::directory_file) {
    m_isLeaf = false;
  } else {
    m_isLeaf = true;
    if (file_status.type() == boost::filesystem::regular_file) {
      size = static_cast<uint64_t>(boost::filesystem::file_size(m_path));
    }
  }
  set_metadata_as("size", size);
  set_metadata_as("time", static_cast<uint64_t>(boost::filesystem::last_write_time(m_path)));
  set_metadata_as("perms", static_cast<uint16_t>(file_status.permissions()));
#if !defined(CINDER_MSW)
  struct stat sb = {0};
  if (::stat(m_path.c_str(), &sb) == 0) {
    set_metadata_as("uid", sb.st_uid);
    set_metadata_as("gid", sb.st_gid);
  }
#endif
}

bool FileSystemNode::is_leaf() const
{
  return m_isLeaf;
}

void FileSystemNode::child_nodes (std::function<bool(const std::shared_ptr<HierarchyNode>&)> callback, FilterCriteria const& filter_criteria)
{
  auto path = m_path;
  auto parent = std::static_pointer_cast<FileSystemNode>(shared_from_this());
  auto thread = boost::thread([callback, parent, path] {
    try {
#if defined(CINDER_MSW)
      // On Windows, our root (/) node's children are the mounted volumes
      if (path == "/") {
        WCHAR volumeName[MAX_PATH + 1] = L"";
        HANDLE handle = FindFirstVolumeW(volumeName, ARRAYSIZE(volumeName));
        if (handle != INVALID_HANDLE_VALUE) {
          do {
            DWORD charCount = MAX_PATH + 1;
            PWCHAR names = nullptr;
            BOOL success = false;

            for (;;) {
              names = new WCHAR[charCount];
              if (!names) {
                break;
              }
              success = GetVolumePathNamesForVolumeName(volumeName, names, charCount, &charCount);
              if (success || GetLastError() != ERROR_MORE_DATA) {
                break;
              }
              delete [] names;
              names = nullptr;
            }
            if (success) {
              for (PWCHAR name = names; name[0] != L'\0'; name += wcslen(name) + 1) {
                try {
                  auto node = std::shared_ptr<FileSystemNode>(new FileSystemNode(boost::filesystem::path(name), parent));
                  node->icon();
                  if (!callback(node)) {
                    break;
                  }
                } catch (...) {}
              }
            }
            delete [] names;
          } while (FindNextVolumeW(handle, volumeName, ARRAYSIZE(volumeName)));
          FindVolumeClose(handle);
        }
      } else
#endif
      if (boost::filesystem::is_directory(path)) {
        boost::filesystem::directory_iterator endIter; // default construction yields past-the-end
        for (boost::filesystem::directory_iterator iter(path); iter != endIter; ++iter) {
          const std::string filename = iter->path().filename().string();
#if !defined(CINDER_MSW)
          if (filename.empty() || filename[0] == '.') { // Ignore dot (hidden) files for now
            continue;
          }
#endif
          try {
            auto node = std::shared_ptr<FileSystemNode>(new FileSystemNode(*iter, parent));
            node->icon();
            if (!callback(node)) {
              break;
            }
          } catch (...) {}
        }
      }
    } catch (...) {}
  });
  thread.detach();
}

std::string FileSystemNode::path() const
{
  const std::string path = m_path.string();
#if defined(CINDER_MSW)
  if (path == "/") {
    return "This PC"; // FIXME
  }
#endif
  return path;
}

ci::Surface8u FileSystemNode::icon()
{
  if (!m_surfaceAttempted) {
    const size_t dimension = 256;
    m_surfaceAttempted = true;
#if defined(CINDER_COCOA)
    @autoreleasepool {
      NSString* filename = [NSString stringWithUTF8String:m_path.string().c_str()];
      NSURL* filenameURL = [NSURL fileURLWithPath:filename];
      NSDictionary* options = [NSDictionary dictionaryWithObject:[NSNumber numberWithBool:NO] forKey:(NSString *)kQLThumbnailOptionIconModeKey];
      NSImage* nsImage = nil;
      CGFloat xoffset = 0, yoffset = 0, width = dimension, height = dimension;

      m_surface = ci::Surface8u(dimension, dimension, true, ci::SurfaceChannelOrder::RGBA);
      unsigned char* dstBytes = m_surface.getData();

      CGImageRef cgImageRef = QLThumbnailImageCreate(kCFAllocatorDefault, (CFURLRef)filenameURL, CGSizeMake(dimension, dimension), (CFDictionaryRef)options);
      if (cgImageRef) {
        width = CGImageGetWidth(cgImageRef);
        height = CGImageGetHeight(cgImageRef);
        xoffset = (dimension - width)/2.0;
        yoffset = (dimension - height)/2.0;
        nsImage = [[NSImage alloc] initWithCGImage:cgImageRef size:NSZeroSize];
        CFRelease(cgImageRef);
      } else {
        nsImage = [[NSWorkspace sharedWorkspace] iconForFile:filename];
      }
      if (nsImage) {
        ::memset(dstBytes, 0, dimension*dimension*4);
        CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
        CGContextRef cgContextRef =
          CGBitmapContextCreate(dstBytes, dimension, dimension, 8, 4*dimension, rgb, kCGImageAlphaPremultipliedLast);
        NSGraphicsContext* gc = [NSGraphicsContext graphicsContextWithGraphicsPort:cgContextRef flipped:NO];
        [NSGraphicsContext saveGraphicsState];
        [NSGraphicsContext setCurrentContext:gc];
        [nsImage drawInRect:NSMakeRect(xoffset, yoffset, width, height) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0];
        [NSGraphicsContext restoreGraphicsState];
        CGContextRelease(cgContextRef);
        CGColorSpaceRelease(rgb);
        m_surface.setPremultiplied(true);
      }
    }
#else
    std::wstring pathNative = m_path.wstring();

    for (size_t i=0; i<pathNative.size(); i++) {
      if (pathNative[i] == '/') {
        pathNative[i] = '\\';
      }
    }

    IShellItemImageFactory *pImageFactory = nullptr;
    if (SUCCEEDED(SHCreateItemFromParsingName(pathNative.c_str(), nullptr, IID_PPV_ARGS(&pImageFactory)))) {
      SIZE sz = { dimension, dimension };
      HBITMAP thumbnail;

      if (SUCCEEDED(pImageFactory->GetImage(sz, SIIGBF_BIGGERSIZEOK | SIIGBF_RESIZETOFIT, &thumbnail))) {
        m_surface = ci::Surface8u(dimension, dimension, true, ci::SurfaceChannelOrder::BGRA);
        GetBitmapBits(thumbnail, 4*dimension*dimension, m_surface.getData());
        DeleteObject(thumbnail);
        m_surface.setPremultiplied(true);
      }
      pImageFactory->Release();
    }
#endif
  }
  return m_surface;
}

bool FileSystemNode::open(std::vector<std::string> const& parameters) const
{
  const std::string path = m_path.string();
  bool opened = false;

  if (!path.empty()) {
#if defined(CINDER_MSW)
    if (is_leaf()) {
      HINSTANCE instance = ShellExecuteA(nullptr, "open", path.c_str(), "", nullptr, SW_SHOWNORMAL);
      opened = true;
    }
#elif defined(CINDER_COCOA)
    CFURLRef urlPathRef;

    if (path[0] != '/' && path[0] != '~') { // Relative Path
      CFBundleRef mainBundle = CFBundleGetMainBundle();
      CFURLRef urlExecRef = CFBundleCopyExecutableURL(mainBundle);
      CFURLRef urlRef = CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, urlExecRef);
      CFStringRef pathRef = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(), kCFStringEncodingUTF8);
      urlPathRef = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault, urlRef, pathRef, false);
      CFRelease(pathRef);
      CFRelease(urlRef);
      CFRelease(urlExecRef);
    } else {
      urlPathRef = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(path.data()), path.size(), false);
    }
    bool isAppBundle = false;
    bool isOtherBundle = false;

    @autoreleasepool {
      NSString* filename = [NSString stringWithUTF8String:m_path.c_str()];
      NSString* canonicalFilename = [NSString stringWithUTF8String:boost::filesystem::canonical(m_path).c_str()];
      NSBundle* bundle = [NSBundle bundleWithPath:filename];
      NSString* execPath = [bundle executablePath];
      NSString* resourcePath = [bundle resourcePath];
      isAppBundle = (execPath != nil && m_path.extension() == ".app");
      isOtherBundle = (resourcePath != nil &&
                       ![filename isEqualToString:resourcePath] &&
                       ![canonicalFilename isEqualToString:resourcePath]);
    }
    if (isAppBundle) {
      FSRef fsRef;

      if (CFURLGetFSRef(urlPathRef, &fsRef)) {
        CFStringRef args[parameters.size()];
        CFArrayRef argv = nullptr;

        if (!parameters.empty()) {
          for (size_t i = 0; i < parameters.size(); i++) {
            args[i] = CFStringCreateWithCString(kCFAllocatorDefault, parameters[i].c_str(), kCFStringEncodingUTF8);
          }
          argv = CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void**>(args), parameters.size(), nullptr);
        }
        LSApplicationParameters params = {0, kLSLaunchDefaults, &fsRef, nullptr, nullptr, argv, nullptr};
        LSOpenApplication(&params, 0);
        if (argv) {
          CFRelease(argv);
        }
        for (size_t i = 0; i < parameters.size(); i++) {
          CFRelease(args[i]);
        }
      }
      opened = true; // Whether or not we actually launched the app, don't go snooping into .app files
    } else if (isOtherBundle || !boost::filesystem::is_directory(m_path)) {
      opened = !LSOpenCFURLRef(urlPathRef, 0);
    }
    CFRelease(urlPathRef);
#endif
  }
  return opened;
}

bool FileSystemNode::move(HierarchyNode& to_parent)
{
  return false;
}

bool FileSystemNode::remove()
{
  return false;
}

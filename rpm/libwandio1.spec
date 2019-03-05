Name:           libwandio1
Version:        4.1.1
Release:        1%{?dist}
Summary:        C Multi-Threaded File Compression and Decompression Library

License:        LGPLv3
URL:            https://github.com/wanduow/wandio
Source0:        https://github.com/wanduow/wandio/archive/%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  zlib-devel
BuildRequires:  lzo-devel
BuildRequires:  bzip2-devel
BuildRequires:  lz4-devel
BuildRequires:  xz-devel
BuildRequires:  libzstd-devel
BuildRequires:  libcurl-devel

%description
File I/O library that will read and write both compressed and uncompressed
files. All compression-related operations are performed in a separate thread
where possible resulting in significant performance gains for tasks where I/O
is the limiting factor (most simple trace analysis tasks are I/O-limited).

libwandio is developed by the WAND Network Research Group at Waikato
University, New Zealand.

%package        devel
Summary:        Development files for %{name}
Requires:       %{name}%{?_isa} = %{version}-%{release}

%package        tools
Summary:        Example tools for the %{name} library
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description    devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.

%description    tools
The %{name}-tools package contains some example tools to demonstrate the
 %{name} library.

%prep
%setup -q -n wandio-%{version}

%build
%configure --disable-static --mandir=%{_mandir}
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
%make_install
find $RPM_BUILD_ROOT -name '*.la' -exec rm -f {} ';'

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%license COPYING
%{_libdir}/*.so.*

%files devel
%{_includedir}/*
%{_libdir}/*.so

%files tools
%doc %{_mandir}/man1/wandiocat.1.gz
%{_bindir}/wandiocat

%changelog
* Thu Feb 21 2019 Shane Alcock <salcock@waikato.ac.nz> - 4.1.0-1
- First libwandio package



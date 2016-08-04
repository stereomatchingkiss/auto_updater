# Synopsis

A simple CLI tool to make the apps able to do auto update.

# Dependencies

The dependencies of this project are

1. [Qt5.6.x](https://www.qt.io/download-open-source/#section-2)
2. [qt_enhance/compressor](https://github.com/stereomatchingkiss/qt_enhance/tree/master/compressor)
3. [QsLog](https://bitbucket.org/codeimproved/qslog)

#How to build them

##Qt5.6.x

Community already build it for us, just download the installer suit your os and compiler.

##qt_enhance/compressor

Include the sources files 

1. file_compressor.cpp
2. folder_compressor.cpp

into your make file

##QsLog

clone the codes by github, include the QsLog.pri in .pro file

#How to use it

This program use xml files and text file to indicate the component you want to update and remove.It will compare local xml file
(update_info_local.xml) and remote xml file(update_info_remote.xml) to determine which component should update.

1. Create a file called "update_info_local.xml", this file should contain the informations of different components you want to update 
and at the same folder of the auto_update binary.
2. Create a file called "update_info_remote.xml", this file should contain the informations of different components you want to update,
place at the remote server specify by "update_info_local.xml".
3. Create a file called "file_list.txt", specify the location(on remote server) of the erase_list.txt
4. Ceate a file called "erase_list.txt" at remote server, this file should contain the absolute paths of the files/folders you want to delete.
5. (Optional) Specify the absolute path of the app you want to start after the update are finished.

##Example--update_info_local.xml

```xml
<?xml version="1.0" encoding="UTF-8"?>
<Installer>	
  <RemoteRepositories>
    <!--Repository group the informations of a component, you must specify-->
	<!--the Url, Version, Unzip, CopyTo and DisplayName-->
	<Repository>
	    <!--Url specify the location of the component-->
	    <Url>https://dl.dropboxusercontent.com/s/2s0qdp8h1ddkd1p/update_info_remote.xml?dl=0</Url>
	    <!--If the version number of local older than remote server, it will update the component from-->
	    <!--the remote server, the component will catch from the Url-->
	    <Version>1.0</Version>
	    <!--Name of the component-->
        <DisplayName>update_info</DisplayName>
        <!--Specify decompress method, empty mean it is raw data, qcompress_folder means it should-->
        <!--decompress by the decompress class of qt_enhance, qcompress_file means it should decompress by qUncompress-->
	    <Unzip></Unzip>
	    <!-- Indicate the component should copy to where, $${PARENT} equal to the parent path of auto_update-->
	    <CopyTo></CopyTo>
   </Repository>	
   <Repository>
	 <Url>https://dl.dropboxusercontent.com/s/g82r2087c9tch32/qt5_dll.zip?dl=0</Url>            
     <Version>1.0</Version>
     <DisplayName>qt5_dll</DisplayName>
     <Unzip>qcompress_folder</Unzip>
	 <CopyTo>$${PARENT}</CopyTo>
   </Repository>
  </RemoteRepositories>
</Installer>
```

##Example--update_info_remote.xml

```xml
<?xml version="1.0" encoding="UTF-8"?>
<Installer>	
  <RemoteRepositories>
    <!--Repository group the informations of a component, you must specify-->
	<!--the Url, Version, Unzip, CopyTo and DisplayName-->
	<Repository>
	    <Url>https://dl.dropboxusercontent.com/s/2s0qdp8h1ddkd1p/update_info_remote.xml?dl=0</Url>
	    <!--version number of remote must larger than local file, else the components will not updated-->
	    <!--If the version number of update_info lower or equal to local one, all of the components will not update-->
	    <Version>1.1</Version>
        <DisplayName>update_info</DisplayName>
	    <Unzip></Unzip>
	    <CopyTo></CopyTo>
   </Repository>	
   <Repository>
	 <Url>https://dl.dropboxusercontent.com/s/g82r2087c9tch32/qt5_dll.zip?dl=0</Url>            
     <Version>1.1</Version>
     <DisplayName>qt5_dll</DisplayName>
     <Unzip>qcompress_folder</Unzip>
	 <CopyTo>$${PARENT}</CopyTo>
   </Repository>
   <Repository>
     <!--This component did not specify in the update_info_local.xml, so this component will be updated-->
	 <Url>https://dl.dropboxusercontent.com/s/g82r2087c9tch32/opencv_dll.zip?dl=0</Url>            
     <Version>1.1</Version>
     <DisplayName>opencv_dll</DisplayName>
     <Unzip>qcompress_folder</Unzip>
	 <CopyTo>$${PARENT}</CopyTo>
   </Repository>
  </RemoteRepositories>
</Installer>
```

##Example--file_list.txt

```
erase_list https://dl.dropboxusercontent.com/s/23e7v210wyzvtms/erase_list.txt?dl=0
```

First column is the name(key) of the file, erase_list means the file pointed by the url(second column) stored the absolute path 
of the files/folders you want to delete.By now file_list.txt only support one type of file(erase_list), in the future I may add more.

##Example--erase_list.txt
```
$${PARENT}/libEGL.dll
$${PARENT}/Qt5Widgets.dll
C:\turf_club_statistic\Qt5Network.dll
```

Every line of the erase_list.txt is the absolute path of the file/folder you want to delete

##Command line examples

Update the apps without restart
```
auto_update
```

Update the apps with auto restart
```
auto_update -a $${PARENT}/similar_vision
```

Show help
```
auto_update --help
```

# Limitation

Do not support authentication, however I do not think this is a big deal, because this tool is designed for open source project.

# Bugs report and features request

If you found any bugs or any features request, please open the issue at github.
I will try to solve them when I have time.

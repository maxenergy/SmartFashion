# SmartFashion


## Install

Support in **Ubuntu 20** or newer and Microsoft **Windows 10**.

### Dependencies
- Qt 5.15.x ([Here](https://www.qt.io/download-open-source), scroll to bottom and download)

**_It is recommanded to use QtCreator as the IDE._**

- OpenCV 4.5 or newer (Prebuilt [Ubuntu 20](https://ext.bravedbrothers.com/OpenCV_ubuntu.tar.gz) / [Windows10](https://ext.bravedbrothers.com/OpenCV.7z))
- OpenMesh 9 or newer (Prebuilt [Ubuntu 20](https://ext.bravedbrothers.com/OpenMesh_ubuntu.tar.gz) / [Windows10](https://ext.bravedbrothers.com/OpenMesh.7z))

#### _Ubuntu_

It is suggested to installed the ffmpeg package for video-IO.

```console
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libx264-dev libavresample-dev -Y
```
___

If pre-builts are used, unzip the files after download. E.g. in Linux
```console
tar -xzvf OpenCV_ubuntu.tar.gz
mv OpenCV_ubuntu OpenCV
tar -xzvf OpenMesh_ubuntu.tar.gz
mv OpenMesh_ubuntu OpenMesh
```

Copy/Move the unzipped folders into the repository directory. Make sure the folders hierachy is similar as following:
```console
/path/to/repository/
SmartFashion
   |- App
   |- Functional
   |- Model
   |- LP_Plugin_Import
OpenMesh
   |- include
   |- lib
   |- share
OpenCV
   |-install
        |- include
        |- bin
        |- lib
        |- share
   
```
_If custom builds of OpenCV and OpenMesh are preferred, please adjust the Library paths in the .pro's accordingly_

## Example Snapshots

#### Plugins -> Mesh Slicer

![Slicer](/images/slicer01.JPG)


#### Tools -> Knitting Instructions

![Knitting Instructions](/images/knitinstruction.JPG)


#### Tools -> YOLO Helper

![YOLO Annotator](/images/yolo01.JPG)

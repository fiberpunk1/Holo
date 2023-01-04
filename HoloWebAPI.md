## 1. API interface description

This document is used to describe the API design of the Node module.

---

### 1.Find Device API

**Brief description**

- Provide an http API for the client to search Holo device, and if Holo receives this network request, it will return its IP
- If it has been connected before, it will return the status of the current device

**Request URL**
- ` http://192.168.1.133/find `

**Request Method**
- GET 

**Parameters**

- None

**Return Example**

```
Fiberpunk:192.168.1.133
```

---

### 2. Get file directory information

**Brief description**

- Get all the file dir information in the SD card

**Request URL**
- ` http://192.168.1.133/list?dir=/3DBenchy `
  
**Request Method**
- GET

**Parameters**
- directory name

**Return Example**


```
[
  {
    "type": "file",
    "name": "/3DBenchy/0.JPG"
  },
  {
    "type": "file",
    "name": "/3DBenchy/1.JPG"
  },
  {
    "type": "file",
    "name": "/3DBenchy/10.JPG"
  },
  {
    "type": "file",
    "name": "/3DBenchy/11.JPG"
  },
  {
    "type": "file",
    "name": "/3DBenchy/2.JPG"
  },
  {
    "type": "file",
    "name": "/3DBenchy/3.JPG"
  },
  {
    "type": "file",
    "name": "/3DBenchy/8.JPG"
  },
  {
    "type": "file",
    "name": "/3DBenchy/9.JPG"
  }
]
```

**Return error code**

"NOT DIR" No directory is generally caused by the Node not being able to obtain the SD card. 

**Return parameter description**

|Parameter Name|Type|Description|
|:-----:  |:-----:|-----                           |
|type |string   |The current type of this name, file or directory|
|name |string   |File or directory name|



---

### 3. Upload files

**Brief description**

- User uploads files

**Request URL**
- ` http://192.168.1.133/edit `
  
**Request Method**
- POST

**Parameters**

- Uploading files as a form

**Return Example**

- Upload completed, return ok

**Return parameter description**

- None

**Remarks**

- None

---

### 4. Delete files

**Brief description**

- User deletes files from the sd card

**Request URL**
- ` http://192.168.1.133:88/delete?path=/xxx.gcode `

**Request Method**
- DELETE

**Parameters**

- Specify path=/xxx.gcode in the url, which is the file to be deleted
- Can delete files and directories

**Return Example**
- Delete completed, return ok

**Return parameter description**
- None

**Return error code**"BAD PATH" means no file exists to change, this error will also appear when Node cannot get the SD card.

**Remarks**
- None

---

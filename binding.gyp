{
    "targets": [
        {
            "target_name": "camera",
            "sources": ["src/native/camera.cpp"],
            "include_dirs": [
                "/opt/local/include"
            ],
            "link_settings": {
                "libraries": ["-lopencv_core", "-lopencv_highgui", "-lopencv_imgproc", "-lopencv_video", "-lopencv_ml"],
                "library_dirs": ["/opt/local/lib"]
            },
            "conditions": [
                ['OS=="mac"', {
                    'xcode_settings' : {
                        'OTHER_CFLAGS' : [
                            "-mmacosx-version-min=10.7",
                            "-std=c++11",
                            "-stdlib=libc++"
                        ],
                            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
                            'GCC_ENABLE_CPP_RTTI': 'YES'
                    }
                }]
            ]
    }
    ]
}
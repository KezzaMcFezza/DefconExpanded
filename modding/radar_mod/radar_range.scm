; Radar Range - 58 pixels blur
(define (my-blurexpand inImage inLayer range brightness)
  (let ((theImage inImage)
        (theLayer inLayer))
    (script-fu-tile-blur theImage theLayer range 1 1 "RLE")
    (gimp-drawable-brightness-contrast theLayer brightness 1.0)
    (script-fu-tile-blur theImage theLayer range 1 1 "RLE")
    (gimp-drawable-brightness-contrast theLayer brightness 1.0)
  )
)

(define (defcon-radar-range inImage inLayer)
  (let* ((theImage inImage)
         (brightness 0.9449)   ; 120/127
         (opacity 24)
         (workLayer (car (gimp-layer-copy inLayer FALSE))))

    ; Add work layer to image
    (gimp-image-insert-layer theImage workLayer 0 0)
    
    ; Blend out the ocean part
    (gimp-drawable-brightness-contrast workLayer -0.4724 1.0)  ; -60/127
    
    ; Blur to radar range: 58 pixels total
    (my-blurexpand theImage workLayer 5 brightness)
    (my-blurexpand theImage workLayer 5 brightness)
    (my-blurexpand theImage workLayer 5 brightness)
    (my-blurexpand theImage workLayer 5 brightness)
    (my-blurexpand theImage workLayer 10 brightness)
    (my-blurexpand theImage workLayer 10 brightness)
    (my-blurexpand theImage workLayer 9 brightness)
    (my-blurexpand theImage workLayer 9 brightness)

    ; Final blur
    (script-fu-tile-blur theImage workLayer 1 1 1 "RLE")
    
    ; Set name and opacity
    (gimp-item-set-name workLayer "Radar Range")
    (gimp-layer-set-opacity workLayer opacity)

    (gimp-displays-flush)
  )
)

(script-fu-register "defcon-radar-range"
                   "Radar Range..."
                   "Creates radar range layer (58px)"
                   "Manuel Moos"
                   "Manuel Moos"
                   "May 2007"
                   "*"
                   SF-IMAGE    "The Image" 0
                   SF-DRAWABLE "The Layer" 0
)

(script-fu-menu-register "defcon-radar-range" 
                        "<Image>/Filters/Defcon")
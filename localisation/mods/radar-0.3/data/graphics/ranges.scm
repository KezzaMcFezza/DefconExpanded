; blurs about the range
(define (my-blurexpand inImage inLayer range brightness )
  (set! theImage inImage)
  (set! theLayer inLayer)
  (script-fu-tile-blur theImage theLayer range 1 1 _"RLE")
  (gimp-brightness-contrast theLayer brightness 127)
  (script-fu-tile-blur theImage theLayer range 1 1 _"RLE")
  (gimp-brightness-contrast theLayer brightness 127)
  )

(define (defcon-finalize theImage theLayer name opacity)
  ; final blur
  (script-fu-tile-blur theImage theLayer 1 1 1 _"RLE")

  ; copy layer
  (let* ((newLayer (car (gimp-layer-copy theLayer 0))))
	(gimp-layer-set-name newLayer name)
	(gimp-layer-set-opacity newLayer opacity)
	(gimp-image-add-layer theImage newLayer 0)
	newLayer)
)

(define (defcon-test theImage theLayer brightness opacity)
  (my-blurexpand theImage theLayer 10 brightness)
  (my-blurexpand theImage theLayer 10 brightness)

  ; copy into radar range layer
  (defcon-finalize theImage theLayer "Range" opacity)
)

(define (defcon-ranges inImage inLayer)
  (set! theImage inImage)
  (set! theLayer inLayer)
  (set! brightness 120)
  (set! opacity 12)
  (set! preScale 2)

  (set! theHeight (car (gimp-drawable-height theLayer)))
  (set! theWidth (car (gimp-drawable-width theLayer)))

  ; double the size for better quality
  ;(gimp-image-scale theImage (* preScale theWidth) (* preScale theHeight) 0 0)

  ; reread sizes
  (set! theHeight (car (gimp-drawable-height theLayer)))
  (set! theWidth (car (gimp-drawable-width theLayer)))

  (set! theTitle (car (gimp-drawable-get-name theLayer)))

  ; first, blend out the ocean part of the territory
  (gimp-brightness-contrast theLayer -60 127)

  ;(defcon-test theImage theLayer brightness opacity)
  ;(defcon-test theImage theLayer brightness opacity)
  ;(defcon-test theImage theLayer brightness opacity)
  ;(defcon-test theImage theLayer brightness opacity)
  ;(defcon-test theImage theLayer brightness opacity)
  ;(defcon-test theImage theLayer brightness opacity)
  ;(defcon-test theImage theLayer brightness opacity)
  ;(defcon-test theImage theLayer brightness opacity)
  ;(defcon-test theImage theLayer brightness opacity)
  ;(defcon-test theImage theLayer brightness opacity)
  
  ; blur to radar range, 58 pixels
  (my-blurexpand theImage theLayer 5 brightness)
  (my-blurexpand theImage theLayer 5 brightness)
  (my-blurexpand theImage theLayer 5 brightness)
  (my-blurexpand theImage theLayer 5 brightness)
  (my-blurexpand theImage theLayer 10 brightness)
  (my-blurexpand theImage theLayer 10 brightness)
  (my-blurexpand theImage theLayer 9 brightness)
  (my-blurexpand theImage theLayer 9 brightness)

  ; copy into radar range layer
  (defcon-finalize theImage theLayer "Radar Range" ( * 2 opacity ) )

  ; blur to bomber range, 71 pixels, 13 more
  (my-blurexpand theImage theLayer 13 brightness)

  ; copy into new range layer
  ; not worth it, it differs too little from the radar range
  (defcon-finalize theImage theLayer "Bomber Range" opacity)

  ; blur to fighter/sub range, 130 pixels, 59 more
  (my-blurexpand theImage theLayer 10 brightness)
  (my-blurexpand theImage theLayer 10 brightness)
  (my-blurexpand theImage theLayer 10 brightness)
  (my-blurexpand theImage theLayer 10 brightness)
  (my-blurexpand theImage theLayer 10 brightness)
  (my-blurexpand theImage theLayer 9 brightness)

  ; copy into new range layer
  (defcon-finalize theImage theLayer "SubFigher Range" opacity)
  
  ; remove the original layer
  (gimp-image-remove-layer theImage theLayer)

  (gimp-displays-flush)
)

(
 script-fu-register "defcon-ranges"
					_"<Image>/Script-Fu/Defcon/Defcon Ranges..."
					"Defcon Ranges"
					"Manuel Moos"
					"Manuel Moos"
					"May 2007"
					""
					SF-IMAGE       "The Image"         0
                    SF-DRAWABLE    "The Layer"         0
)
					

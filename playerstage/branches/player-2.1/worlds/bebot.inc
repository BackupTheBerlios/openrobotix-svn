# Desc: Device definitions for Bebot robots
# Author: Stefan Herbrechtsmeier
# Date: 14 July 2009


define bebot_ir ranger
(
  # define the number of sensors
  scount 12
  
  # define the pose of each sensor [xpos ypos heading]
  spose[0]  [  0.0455  0.014 0 ]
  spose[1]  [  0.0425  0.041 45 ]
  spose[2]  [  0.0195  0.044 90 ]
  spose[3]  [ -0.0195  0.044 90 ]
  spose[4]  [ -0.0425  0.041 135 ]
  spose[5]  [ -0.0455  0.014 180 ] 
  spose[6] [ -0.0455 -0.014 -180 ]
  spose[7] [ -0.0425 -0.041 -135 ]
  spose[8]  [ -0.0195 -0.044 -90 ]
  spose[9]  [  0.0195 -0.044 -90 ]
  spose[10]  [  0.0425 -0.041 -45 ]
  spose[11]  [  0.0455 -0.014 0 ]

  # define the field of view of each sensor [range_min range_max view_angle]
  sview [ 0.02 0.07 30 ]
  
  # define the size of each sensor [xsize ysize] in meters
  ssize [ 0.001 0.002 ]
)

define bebot position
(
  # actual size
  size [0.091 0.088 0.09]

  # the center of rotation is offset from its center of area
  origin [0.0 0.0 0]

  # draw a nose on the robot so we can see which way it points
  gui_nose 0
  
  # estimated mass in KG
  mass 1.0 

  block
  (
    points 8
    point[7] [ -0.0455  0.0390 ]
    point[6] [ -0.0450  0.0440 ]
    point[5] [  0.0450  0.0440 ]
    point[4] [  0.0455  0.0390 ]
    point[3] [  0.0455 -0.0390 ]
    point[2] [  0.0450 -0.0440 ]
    point[1] [ -0.0450 -0.0440 ]
    point[0] [ -0.0455 -0.0390 ]
    z [0.037 0.090]
    color "gray50"
  )

  block
  (
    points 3
    point[2] [  0.0255 -0.0000 ]
    point[1] [  0.0455  0.0200 ]
    point[0] [  0.0455 -0.0200 ]
    z [0.09 0.091]
    color "red"
  )

  block
  (
    points 4
    point[3] [  0.0455  0.0290 ]
    point[2] [  0.0455 -0.0290 ]
    point[1] [ -0.0455 -0.0290 ]
    point[0] [ -0.0455  0.0290 ]
    z [0.007 0.037]
    color "gray50"
  )

  block
  (
    points 4
    point[3] [  0.0455 -0.0300 ]
    point[2] [  0.0455 -0.0440 ]
    point[1] [ -0.0455 -0.0440 ]
    point[0] [ -0.0455 -0.0300 ]
    z [0.000 0.030]
    color "gray75"
  )

  block
  (
    points 4
    point[0] [  0.0455  0.0300 ]
    point[1] [  0.0455  0.0440 ]
    point[2] [ -0.0455  0.0440 ]
    point[3] [ -0.0455  0.0300 ]
    z [0.000 0.030]
    color "gray75"
  )

  # steering model (diff, omni, car)
  drive "diff"

  localization "odom"
  localization_origin [ 0 0 0 ]
  odom_error [ 0.03 0.03 0.05 ]

  # use the ir array defined above
  bebot_ir()
)

#########################################
#					#
# Group 03-04-01 CG1112 Alex Project    #
#					#
#########################################

######################################################################
## Total number of non-studio software augments as of final version ##
######################################################################
1) Off-load of hectorheightmapping ROS node to Operator Laptop [See Laptop/slam/]
   a) In /Miscellaneous configuration, <[PI].bashrc> and <[Laptop].bashrc> contains statements that
      assigns the operator laptop as the ROS master.

2) Calibration of hector mapping settings [See Laptop/slam/src/rplidar_ros/launch/hectormapping.launch]

3) Rplidar motor-off sleep mode [See Pi/slam/src/rplidar_ros/src/node_alex.cpp and Pi/tls-server-lib/tls-alex-server.cpp]
   a) The operator can shut off the RPLidar Motor manually with 'z' key in the TLS client
   b) The operator can toggle operation to RPLidar sleep mode. 
      - The robot will move even more slowly; the RPLidar will automatically power off when the robot stops.

4) WASD movement keys [See Laptop/tls-client-lib/alex-pi_tls.cpp, writeThread()]
   a) The operator can toggle between default studio implementation for robot control, and "WASD" mode for precise maneuvering.
   b) "WASD" mode allows the robot to move at the optimal speed for mapping, and allows step-by-step turning to facilitate mapping.
   
5) PD motor control [See Arduino/Alex_Arduino/Alex_Arduino.ino]
   a) Motor movement correction using tick values from dual wheel encoders

##############################
## Studio-specific augments ##
##############################
1) Bare Metal - PWM Motor implementation
2) Bare Metal implementation - USART Serial communication
3) Power saving: Arduino 
4) Power saving: Pi
5) TLS client-server implementation

##############################
##  Miscellaneous Folder    ##
##############################
1) Shell scripts for convenient startup of client and server. Includes compilation of .cpp file and execution of output file.
2) .bashrc file to show default commands there are executed when a new terminal loads up

##############################
##   Notes for operation    ##
##############################
1) Need to edit the following files to run the program:
   - /Laptop/tls-client-lib/tls-alex-client: IP address
   - /Laptop/start-tls-client.sh: IP address
   - /Miscellaneous Configurations/ ROS MASTER URI, ROS IP URI
2) Best to copy the following files to your slam folder:
   [Need to run catkin_make to compile and make effective the new changes]
	- hectormapping.launch [/Laptop/slam/src/rplidar_ros/launch/]
	- CMakeLists.txt [/Pi/slam/src/rplidar_ros/launch/]
	- node-alex.cpp [/Pi/slam/src/rplidar_ros/src/]
     

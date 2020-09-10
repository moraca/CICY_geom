//===========================================================================
//SOFTWARE:     3D geometric model for CNT and GS networks
//OBJECTIVE:    Generate 3D nanoparticle network
//AUTHOR:       Angel Mora
//E-MAIL:       angel.mora@cicy.mx
//===========================================================================

#include "GenNetwork.h"

//NOTE ON PERIODICTY (Jul 16 2017):
//Periodicity was removed as it has not been used in any of the updates of the code since it was uploaded to github
//If periodicity needs to be added back again, then it should deal with the following:
//1) Make the extended domain equal to the composite domain. Periodicity solves the same issue as the extended domain when it comes to homegeneity of CNT distribution.
//2) Calculate the intersection with the boundary. This operation is completely useless with a non-periodic sample so I just deleted it.
//3) When adding the new_cnt vector to the global vector of CNTs, split it into segments. This operation is completely useless with a non-periodic sample and was causing some errors when using the penetrating model so I just deleted it.

//Generate 3D nantube networks with ovelapping
int GenNetwork::Generate_nanofiller_network(const struct Simu_para &simu_para, const struct Geom_sample &geom_sample, const struct Agglomerate_Geo &agg_geo, const struct Nanotube_Geo &nanotube_geo, const struct GNP_Geo &gnp_geo, const struct Cutoff_dist &cutoffs, const struct Tecplot_flags &tec360_flags, vector<Point_3D> &cpoints, vector<Point_3D> &gpoints, vector<GCH> &hybrid_particles, vector<double> &cnts_radius, vector<vector<long int> > &cstructures, vector<vector<long int> > &gstructures)const
{
    //Define a two-dimensional vector of three-dimensional points for storing the CNT threads
    vector<vector<Point_3D> > cnts_points;
    //Define a two-dimensional vector of three-dimensional points for storing the GNP discretizations
    vector<vector<Point_3D> > gnps_points;
    
    double carbon_vol = 0, carbon_weight = 0;
    if (simu_para.particle_type == "CNT_wires") {
        //Generate a network defined by points and connections
        //Use the Mersenne Twister for the random number generation
        if (!Generate_cnt_network_threads_mt(simu_para, geom_sample, agg_geo, nanotube_geo, cutoffs, cnts_points, cnts_radius)) {
            hout << "Error in generating a CNT network" << endl;
            return 0;
        }
        
    } else if (simu_para.particle_type == "GNP_cuboids") {
        //Generate a network defined by cuboids and points
        if (!Generate_gnp_network_mt(gnp_geo, geom_sample, cutoffs, simu_para.particle_type, gnps_points, hybrid_particles, carbon_vol, carbon_weight)) {
            hout << "Error in generating a GNP network" << endl;
            return 0;
        }
        
    } else if (simu_para.particle_type == "Hybrid_particles") {
        //Generate a network defined by cuboids and points
        if (!Generate_gnp_network_mt(gnp_geo, geom_sample, cutoffs, simu_para.particle_type, gnps_points, hybrid_particles, carbon_vol, carbon_weight)) {
            hout << "Error in generating a GNP network" << endl;
            return 0;
        }
        if (!Generate_cnt_network_threads_over_gnps_mt(gnp_geo, geom_sample, nanotube_geo, cutoffs, cnts_points, gnps_points, hybrid_particles, cnts_radius)) {
            hout << "Error in generating a CNT network on GNPs" << endl;
            return 0;
        }
        
    } else if (simu_para.particle_type == "GNP_CNT_mix") {
        //Generate a network defined by cuboids and points
        if (!Generate_gnp_network_mt(gnp_geo, geom_sample, cutoffs, simu_para.particle_type, gnps_points, hybrid_particles, carbon_vol, carbon_weight)) {
            hout << "Error in generating a GNP network" << endl;
            return 0;
        }
        //Generate a network defined by points and connections
        //Use the Mersenne Twister for the random number generation
        //==========================
        //GNP_CNT_mix works only with penetrating model in CNTs (oct 30 2016)
        //WILL BE CHANGED FOR THE HYBRID PARTICLE FUNCTION?
        if (!Generate_cnt_network_threads_mt(simu_para, geom_sample, agg_geo, nanotube_geo, cutoffs, cnts_points, cnts_radius)) {
            hout << "Error in generating a CNT network mixed with GNPs" << endl;
            return 0;
        }

    } else {
        hout << "Error: the type of particles shoud be one of the following: CNT_wires, GNP_cuboids, Hybrid_particles or GNP_CNT_mix. Input value was: " << simu_para.particle_type << endl;
        return 0;
    }
    
    //Checking the angle between two segments in one nanotube (if less than PI/2, provide an alarm)
    //if(CNTs_quality_testing(cnts_points)==0) return 0;
    
    //-----------------------------------------------------------------------------------------------------------------------------------------
    //Transform the 2D cnts_points into 1D cpoints and 2D cstructures
    if(Transform_points("CNTs", cnts_points, cpoints, cstructures)==0) return 0;
    //Transform the 2D gnps_points into 1D gpoints and 2D gstructures
    if(Transform_points("GNPs", gnps_points, gpoints, gstructures)==0) return 0;
    
    //-----------------------------------------------------------------------------------------------------------------------------------------
    //Check if Tecplot visualization files were requested for CNTs
    if (tec360_flags.generated_cnts) {
        
        //A new Tecplot_Export object
        Tecplot_Export *Tecexpt = new Tecplot_Export;
        
        //Generate a cuboid for RVE
        struct cuboid cub;
        cub.poi_min = geom_sample.ex_origin;
        cub.len_x = geom_sample.ex_len;
        cub.wid_y = geom_sample.ey_wid;
        cub.hei_z = geom_sample.ez_hei;
        
        if (tec360_flags.generated_cnts == 1) {
            
            //The geometric structure of CNT network (by threads in Tecplot)
            if(Tecexpt->Export_network_threads(cub, cnts_points)==0) return 0;
        }
        else {
            
            //The geometric structure of CNT network (by tetrahedron meshes in Tecplot) //Attention: little parts of nanotube volumes out of the cuboid
            if(Tecexpt->Export_cnt_network_meshes(tec360_flags.generated_cnts, cub, cnts_points, cnts_radius)==0) return 0;
        }
        
        delete Tecexpt;
    }
 
    //-----------------------------------------------------------------------------------------------------------------------------------------
    //Check if Tecplot visualization files were requested for GNPs
    if (tec360_flags.generated_gnps) {
        
        //A new Tecplot_Export object
        Tecplot_Export *Tecexpt = new Tecplot_Export;
        
        ofstream otec("GNP_cuboids.dat");
        otec << "TITLE = GNP_cuboids" << endl;
        otec << "VARIABLES = X, Y, Z" << endl;
        vector<int> cluster_gch(hybrid_particles.size(), 0);
        for (int i = 1; i < (int)hybrid_particles.size(); i++)
            cluster_gch[i] = i;
        struct cuboid gvcub;
        gvcub.poi_min = geom_sample.origin;
        gvcub.len_x = geom_sample.len_x;
        gvcub.wid_y = geom_sample.wid_y;
        gvcub.hei_z = geom_sample.hei_z;
        if(Tecexpt->Export_cuboid(otec, gvcub)==0) return 0;
        string zone_name = "as_generated";
        if(Tecexpt->Export_randomly_oriented_gnps(otec, hybrid_particles, cluster_gch,zone_name)==0) return 0;
        delete Tecexpt;
        otec.close();
    }
    
    return 1;
}
//Generate a network defined by points and connections
//Use the Mersenne Twister for the random number generation
int GenNetwork::Generate_cnt_network_threads_mt(const struct Simu_para &simu_para, const struct Geom_sample &geom_sample, const struct Agglomerate_Geo &agg_geo, const struct Nanotube_Geo &nanotube_geo, const struct Cutoff_dist &cutoffs, vector<vector<Point_3D> > &cnts_points,  vector<double> &cnts_radius)const
{
    // Use random_device to generate seeds for Mersenne twister engines.
    std::random_device rd;
    
    //Initial seeds, if any are in network_seeds within geom_sample.
    //However, geom_sample cannot be modified, so copy the seeds to a new vector
    vector<unsigned int> net_seeds(7, 0);
    
    //If seeds have been specified, copy them
    if (simu_para.network_seeds.size()) {
        for (size_t i = 0; i < simu_para.network_seeds.size(); i++) {
            net_seeds[i] = simu_para.network_seeds[i];
        }
    }
    //If seeds have not been specified, generate them
     else {
        
        //7 seeds are needed
        net_seeds.assign(7, 0);
        
        //Generate all the new seeds and print them to the output file
        net_seeds[0] = rd();
        hout << "seed x: "<<net_seeds[0]<<endl;
        net_seeds[1] = rd();
        hout << "seed y: "<<net_seeds[1]<<endl;
        net_seeds[2] = rd();
        hout << "seed z: "<<net_seeds[2]<<endl;
        net_seeds[3] = rd();
        hout << "seed pha: "<<net_seeds[3]<<endl;
        net_seeds[4] = rd();
        hout << "seed sita: "<<net_seeds[4]<<endl;
        net_seeds[5] = rd();
        hout << "seed rand: "<<net_seeds[5]<<endl;
        net_seeds[6] = rd();
        hout << "seed init dir: "<<net_seeds[6]<<endl;
        
    }
    
    //Use the seeds generated above
    std::mt19937 engine_x(net_seeds[0]);
    std::mt19937 engine_y(net_seeds[1]);
    std::mt19937 engine_z(net_seeds[2]);
    std::mt19937 engine_pha(net_seeds[3]);
    std::mt19937 engine_sita(net_seeds[4]);
    std::mt19937 engine_rand(net_seeds[5]);
    std::mt19937 engine_initial_direction(net_seeds[6]);
    
    // "Filter" MT's output to generate double values, uniformly distributed on the closed interval [0, 1].
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    // "Filter" MT's output to generate double values, uniformly distributed on the closed interval [-1, 1].
    std::uniform_real_distribution<double> dist_initial(-1.0, 1.0);

    //---------------------------------------------------------------------------
    //Time variables to keep track of generation time
    time_t ct0, ct1;
    
    //---------------------------------------------------------------------------
    //Generate the data for nanotube clusters limited in ellipsoid surfaces (each ellipsoid is within the RVE and is separated with each other)
    if(agg_geo.vol_fra_criterion>0.0)
    {
        struct cuboid cub;										//Generate a cuboid for RVE
        cub.poi_min = geom_sample.origin;
        cub.len_x = geom_sample.len_x;
        cub.wid_y = geom_sample.wid_y;
        cub.hei_z = geom_sample.hei_z;
        cub.volume = cub.len_x*cub.wid_y*cub.hei_z;
        
        if(Get_ellip_clusters(cub, agg_geo)==0) return 0;
        //Generate a number of sperical clusters in regular arrangement
        //		if(Get_spherical_clusters_regular_arrangement(cub, agg_geo)==0) return 0;
    }
    
    //---------------------------------------------------------------------------
    double vol_sum = 0;  //the sum of volume of generated CNTs
    double wt_sum = 0;   //the sum of weight of generated CNTs
    int cnt_seed_count = 0; //to record the number of generated seeds of a CNT (If the growth of a CNT fails, but this seed will be used for the next growth of a CNT)
    int cnt_reject_count = 0; //to record the number of CNTs that were deleted due to penetration
    int point_overlap_count = 0; //to record the number of times that a point had to be relocated
    int point_overlap_count_unique = 0; //to record the number of points that were overlapping other points
    
    //---------------------------------------------------------------------------
    //Vectors for handling CNT penetration
    //global_coordinates[i][0] stores the CNT number of global point i
    //global_coordinates[i][1] stores the local point number of global point i
    vector<vector<int> > global_coordinates;
    //sectioned_domain[i] contains all the points in sub-region i.
    //Sub-region i is an overlapping subregion to check for penetrations
    vector<vector<long int> > sectioned_domain;
    //n_subregions[0] is the number of subregions along x
    //n_subregions[1] is the number of subregions along y
    //n_subregions[2] is the number of subregions along z
    vector<int> n_subregions;
    //Define cutoff for overlapping
    double overlap_max_cutoff = 2*nanotube_geo.rad_max + cutoffs.van_der_Waals_dist;
    //Initialize the vector sub-regions
    Initialize_subregions(geom_sample, n_subregions, sectioned_domain);
    //This flag will be used to skip overlapping functions
    //1 = non-penetrating model
    //0 = penetrating model
    int penetrating_model_flag = 1;
    
    //Generate cuboids that represent the extended domain and the composite domain
    //To calculate the effective portion (length) which falls into the given region (RVE)
    struct cuboid gvcub;					//generate a cuboid to represent the composite domain
    gvcub.poi_min = geom_sample.origin;
    gvcub.len_x = geom_sample.len_x;
    gvcub.wid_y = geom_sample.wid_y;
    gvcub.hei_z = geom_sample.hei_z;
    gvcub.volume = geom_sample.volume;
    struct cuboid excub;					//generate a cuboid to represent the extended domain
    excub.poi_min = geom_sample.ex_origin;
    excub.len_x = geom_sample.ex_len;
    excub.wid_y = geom_sample.ey_wid;
    excub.hei_z = geom_sample.ez_hei;
    excub.volume = excub.len_x*excub.wid_y*excub.hei_z;
    
    //Get the time when generation started
    ct0 = time(NULL);
    //Check when 10% is completed
    double vol_completed = 0.1;
    
    //---------------------------------------------------------------------------
    while((nanotube_geo.criterion == "vol"&&vol_sum < nanotube_geo.real_volume)||
          (nanotube_geo.criterion == "wt"&&wt_sum < nanotube_geo.real_weight))
    {
        //---------------------------------------------------------------------------
        //Define a vector for a new nanotube
        vector<Point_3D> new_cnt;
        int new_cnt_size = (int)new_cnt.size();
        
        //---------------------------------------------------------------------------
        //Randomly generate a length of a CNT
        double cnt_length;
        if(Get_random_value_mt(nanotube_geo.len_distrib_type, engine_rand, dist, nanotube_geo.len_min, nanotube_geo.len_max, cnt_length)==0) return 0;
        //Calculate the total number of growth step for a CNT
        int step_num = (int)(cnt_length/nanotube_geo.step_length) + 1;
        
        //---------------------------------------------------------------------------
        //Randomly generate a radius of a CNT
        double cnt_rad;
        if(!Get_random_value_mt(nanotube_geo.rad_distrib_type, engine_rand, dist, nanotube_geo.rad_min, nanotube_geo.rad_max, cnt_rad)) {
            hout << "Error in Generate_network_threads_mt when calling Get_random_value_mt" <<endl;
            return 0;
        }
        
        //---------------------------------------------------------------------------
        //Randomly generate an initial direction, then generate the rotation matrix that results in that rotation
        MathMatrix multiplier(3,3);
        if (!Get_initial_direction_mt(nanotube_geo.dir_distrib_type, nanotube_geo.ini_sita, nanotube_geo.ini_pha, engine_initial_direction, dist_initial, multiplier)) {
            hout << "Error in Generate_network_threads_mt when calling Get_initial_direction_mt" <<endl;
            return 0;
        }
        
        //---------------------------------------------------------------------------
        //The increased volume of each segement (growth step) of nanotube (Here the overlapping volume is ignored)
        const double step_vol_para = PI*cnt_rad*cnt_rad;
        //---------------------------------------------------------------------------
        //The increased weight of each segement (growth step) of nanotube (If the different radii of nanotube are considered, the linear_density may be different in every nanotube)
        const double step_wei_para = nanotube_geo.linear_density;
        
        //---------------------------------------------------------------------------
        //Randomly generate a seed (initial point) of a CNT in the extended RVE
        Point_3D cnt_poi;
        if(Get_seed_point_mt(excub, cnt_poi, engine_x, engine_y, engine_z, dist)==0) return 0;
        
        //Check overlapping of the intial point
        int counter = 1;
        while (penetrating_model_flag && !Check_penetration(geom_sample, nanotube_geo, cnts_points, global_coordinates, sectioned_domain, cnts_radius, new_cnt, n_subregions, cnt_rad, cutoffs.van_der_Waals_dist, point_overlap_count, point_overlap_count_unique, cnt_poi)) {
            if(Get_seed_point_mt(excub, cnt_poi, engine_x, engine_y, engine_z, dist)==0) return 0;
            cnt_seed_count++;					//record the number of seed generations
            //hout << "Seed deleted" << endl;
            if (counter == MAX_ATTEMPTS) {
                hout << "Too many attempts to resolve overlapping of an intial CNT point (" << counter << " attempts). ";
                hout << cnt_poi.x << ' ' << cnt_poi.y << ' ' << cnt_poi.z << endl;
                return 0;
            }
            counter ++;
        }//*/
        
        new_cnt.push_back(cnt_poi);	//store this seed point in the vector for a new nanotube
        
        //---------------------------------------------------------------------------
        cnt_seed_count++;					//record the number of seed generations
        //hout << "Seed="<<cnt_seed_count<<endl;
        int max_seed = 1E9;
        if(cnt_seed_count>max_seed)
        {
            hout << "The number of seed genrations is lager than "<<max_seed<<", but the nanotube generation still fails to acheive the demanded volume fraction." << endl;
            return 0;
        }
        
        //---------------------------------------------------------------------------
        //
        
        //---------------------------------------------------------------------------
        //The growth process of nanotube
        int ellip_num = -1; //For recording the serial number of ellipsoid cluster which a nanotube penetrates out. It is no use if the cluster generation is not considered.
        for(int i=0; i<step_num; i++)
        {
            //Randomly generate a direction in the spherical coordinates
            //To have the positive Z-axis to be a central axis
            //Then, the radial angle, sita, obeys a normal distribution (sita \in fabs[(-omega,+omega)]) and the zonal angle, pha, obeys a uniform distribution (pha \in (0,2PI))
            double cnt_sita = 0, cnt_pha = 0;
            if(Get_normal_direction_mt(nanotube_geo.angle_max, cnt_sita, cnt_pha, engine_sita, engine_pha, dist)==0) return 0;
            
            //To calculate the new multiplier for transformation of coordinates
            multiplier = multiplier*Get_transformation_matrix(cnt_sita, cnt_pha);
            
            //hout << "i="<<i<<"<"<<step_num<<endl;
            cnt_poi = cnt_poi + Get_new_point(multiplier, nanotube_geo.step_length);
            cnt_poi.flag = 1;							//1 means that point is not the intial point
            
            //---------------------------------------------------------------------------
            //If a CNT penetrates the ellipsoidal surface of a cluster from inside, the growth of this CNT will be headed back in the cluster in a probability p, (0<=p<=1).
            if(agg_geo.ellips.size()>0)
            {
                return 0;
            }
            
            //---------------------------------------------------------------------------
            //If the new CNT point grows out of the extended domain, terminate generation of the CNT
            bool touch_end = false;
            //hout <<"point in RVE"<<endl;
            if(Judge_RVE_including_point(excub, cnt_poi)==0)
            {
                //Break the for-loop
                touch_end = true;
            }
            
            //---------------------------------------------------------------------------
            //Check for overlapping
            //hout << "Check overlapping ";
            if (!penetrating_model_flag || Check_penetration(geom_sample, nanotube_geo, cnts_points, global_coordinates, sectioned_domain, cnts_radius, new_cnt, n_subregions, cnt_rad, cutoffs.van_der_Waals_dist, point_overlap_count, point_overlap_count_unique, cnt_poi)) {
                
                //---------------------------------------------------------------------------
                //If the overlapping model is used or if overlapping was successfully solved, then proceed as usual
                //---------------------------------------------------------------------------
                
                double temp_length = Effective_length_given_region(gvcub, new_cnt.back(),cnt_poi);
                //double temp_length = new_cnt.back().distance_to(cnt_poi);
                if (temp_length > 0.0)
                {
                    vol_sum += temp_length*step_vol_para;		//a accumulation on the volume
                    wt_sum += temp_length*step_wei_para;		//a accumulation on the weight
                }
                
                new_cnt.push_back(cnt_poi);							//store a new point
                new_cnt_size = (int)new_cnt.size()-1;				//calculate the size of new point
            } else {
                //---------------------------------------------------------------------------
                //If the overlapping was not solved, then delete the current CNT and generate a new one
                //---------------------------------------------------------------------------
                //hout << "Penetrating point could not be accommodated" <<endl;
                //Remove the volume and weight fractions corresponding to the new_cnt
                double temp_length;
                for(int j=0; j<(int)new_cnt.size()-1; j++) {
                    if(new_cnt[j+1].flag!=0) {
                        //Calculate segment length
                        temp_length = Effective_length_given_region(gvcub, new_cnt.back(),cnt_poi);
                        //new_cnt[j].distance_to(new_cnt[j+1]);
                        //Subtract corresponding volume
                        vol_sum -= temp_length*step_vol_para;
                        //Subtract corresponding weight
                        wt_sum -= temp_length*step_wei_para;
                    }
                }
                
                //Clear the new_cnt vector so that it is not added to the rest of CNTs
                new_cnt.clear();
                
                //Increase the count of rejected cnts
                cnt_reject_count++;
                
                //Break the for-loop
                touch_end = true;
            }
            //hout << "done" << endl;
            
            
            //---------------------------------------------------------------------------
            //Judge the new volume or weight
            if(nanotube_geo.criterion == "vol"&&vol_sum >= nanotube_geo.real_volume) break;		//Break out when the volume reaches the critical value
            else if(nanotube_geo.criterion == "wt"&&wt_sum >= nanotube_geo.real_weight) break;		//Break out when the weight reaches the critical value
            else if (touch_end) break; //Enforce break
        }
        
        //---------------------------------------------------------------------------
        //Store the CNT points
        //hout << "Store CNT ";
        if(new_cnt.size() >= 2)
        {
            //If the new_cnt vector has at least two points, then it can be added to the rest of the points
            cnts_points.push_back(new_cnt);
            
            //Add the radius to the vector of radii
            cnts_radius.push_back(cnt_rad);
            
            //Perform these operations when the non-overlapping model is used
            if (penetrating_model_flag) {
                
                //Variable needed for updating global_coordinates
                vector<int> empty;
                
                int CNT = (int)cnts_points.size()-1;
                
                //Scan all points in the CNT that was just generated
                for(int ii=0; ii<(int)new_cnt.size(); ii++)
                {
                    
                    //Add global coordinate
                    global_coordinates.push_back(empty);
                    global_coordinates.back().push_back(CNT);
                    global_coordinates.back().push_back(ii);
                    //Add point to an overlapping region in the vector sectioned_domain
                    Add_to_overlapping_regions(geom_sample, overlap_max_cutoff, new_cnt[ii], (long int)global_coordinates.size()-1, n_subregions, sectioned_domain);
                    
                }
            }
        }
        //hout << "done"<<endl;
        //(nanotube_geo.criterion == "vol"&&vol_sum < nanotube_geo.real_volume)||
        //(nanotube_geo.criterion == "wt"&&wt_sum < nanotube_geo.real_weight)
        //(nanotube_geo.criterion == "wt"&&wt_sum > vol_completed*nanotube_geo.real_weight)
        //Check progress
        if ( (nanotube_geo.criterion == "vol"&&vol_sum > vol_completed*nanotube_geo.real_volume)) {
            //Get the time
            ct1 = time(NULL);
            
            //Output elapsed time
            hout << "Completed " << vol_completed*100 << " % of target volume. Elapsed time: " << (int)(ct1-ct0) << " secs." <<endl;
            
            //When the next 10% is completed send another message
            vol_completed = vol_completed + 0.1;
        }
        
    }
    
    //Output the CNT content generated
    if(nanotube_geo.criterion == "wt") {
        hout << "The weight fraction of generated CNTs is: " << wt_sum/(geom_sample.volume*geom_sample.polymer_density) << endl;
        hout << ", the target weight fraction was " << nanotube_geo.weight_fraction << endl << endl;

    } else if(nanotube_geo.criterion == "vol") {
        hout << endl << "The volume fraction of generated CNTs was " << vol_sum/geom_sample.volume;
        hout << ", the target volume fraction was " << nanotube_geo.volume_fraction << endl << endl;
    }
    
    //============================================================================
    //=========== TEST CODE
    
    /*/ROTATE THE X-AXIS SO THAT THE Y-AXIS BECOMES THE Z-AXIS
    double Px, Py, Pz;
    for (int i = 0; i < (int)cnts_points.size(); i++) {
        for (int j = 0; j < (int)cnts_points[i].size(); j++) {
            //Save original coordinates
            Px = cnts_points[i][j].x;
            Py = cnts_points[i][j].y;
            Pz = cnts_points[i][j].z;
            //Apply rotation
            cnts_points[i][j].x = Px;
            cnts_points[i][j].y = -Pz + geom_sample.wid_y;
            cnts_points[i][j].z = Py;
        }
    }//*/
    
    //=========== TEST CODE
    //============================================================================
    
    hout << "There were " << point_overlap_count_unique << " overlapping points and ";
    hout << point_overlap_count << " overlaps, " << cnt_reject_count << " CNTs were rejected." << endl;
    
    return 1;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//Generate a GNP network
//Use the Mersenne Twister for the random number generation
int GenNetwork::Generate_gnp_network_mt(const struct GNP_Geo &gnp_geo, const struct Geom_sample &geom_sample, const struct Cutoff_dist &cutoffs, const string &particle_type, vector<vector<Point_3D> > &gnps_points, vector<GCH> &hybrid_praticles, double &carbon_vol, double &carbon_weight)const
{
    //---------------------------------------------------------------------------
    //Set up the Mersenne Twisters used for the different variables
    // Use random_device to generate a seed for Mersenne twister engine.
    std::random_device rd;
    //---------------------------------------------------------------------------
    //Generate differnet engines for different variables
    std::mt19937 engine_x(rd());
    std::mt19937 engine_y(rd());
    std::mt19937 engine_z(rd());
    std::mt19937 engine_l(rd());
    std::mt19937 engine_t(rd());
    std::mt19937 engine_orientation(rd());
    
    // "Filter" MT's output to generate double values, uniformly distributed on the closed interval [0, 1].
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    // "Filter" MT's output to generate double values, uniformly distributed on the closed interval [-1, 1].
    std::uniform_real_distribution<double> dist_orientation(-1.0, 1.0);
    
    //---------------------------------------------------------------------------
    double vol_sum = 0;  //the sum of volume of generated GNPs
    double wt_sum = 0;   //the sum of weight of generated GNPs
    int gnp_count = 0; //to record the number of successfuly generated hybrid particles
    int gnp_reject_count = 0; //to record the number of CNTs that were deleted due to penetration
    
    //---------------------------------------------------------------------------
    //Vectors for handling CNT penetration
    //global_coordinates[i][0] stores the CNT number of global point i
    //global_coordinates[i][1] stores the local point number of global point i
    vector<vector<int> > global_coordinates;
    //sectioned_domain[i] contains all the points in sub-region i, a negative number indicates the discretized GNP
    //Sub-region i is an overlapping subregion to check for penetrations
    vector<vector<long int> > sectioned_domain;
    //n_subregions[0] is the number of subregions along x
    //n_subregions[1] is the number of subregions along y
    //n_subregions[2] is the number of subregions along z
    vector<int> n_subregions;
    //Initialize the vector sub-regions
    Initialize_subregions(geom_sample, n_subregions, sectioned_domain);
    //This flag will be used to skip overlapping functions
    //1 = non-penetrating model
    //0 = penetrating model
    int penetrating_model_flag = 1;
    
    //---------------------------------------------------------------------------
    //Generate cuboids that represent the extended domain and the composite domain
    //To calculate the effective portion (length) which falls into the given region (RVE)
    struct cuboid gvcub;					//generate a cuboid to represent the composite domain
    gvcub.poi_min = geom_sample.origin;
    gvcub.len_x = geom_sample.len_x;
    gvcub.wid_y = geom_sample.wid_y;
    gvcub.hei_z = geom_sample.hei_z;
    struct cuboid excub;					//generate a cuboid to represent the extended domain
    excub.poi_min = geom_sample.origin - Point_3D(gnp_geo.len_max/2,gnp_geo.len_max/2,gnp_geo.len_max/2);
    excub.len_x = geom_sample.len_x + gnp_geo.len_max;
    excub.wid_y = geom_sample.wid_y + gnp_geo.len_max;
    excub.hei_z = geom_sample.hei_z + gnp_geo.len_max;
    excub.volume = excub.len_x*excub.wid_y*excub.hei_z;
    
    gvcub.volume = geom_sample.volume;
    
    //---------------------------------------------------------------------------
    while((gnp_geo.criterion == "vol"&&vol_sum < gnp_geo.real_volume)||
          (gnp_geo.criterion == "wt"&&wt_sum < gnp_geo.real_weight))
    {
        //---------------------------------------------------------------------------
        //Generate a GNP
        GCH hybrid;
        Point_3D gnp_poi;                   //Random displacement of the GNP
        int attempts = 0;
        int success_gnp = 0;
        
        //Randomly assign a geometry to the GNP
        if (!Generate_gnp(gnp_geo, hybrid.gnp, engine_l, engine_t, dist)) {
            hout << "Error in Generate_gnp_network_mt when calling Generate_gnp" << endl;
            return 0;
        }
        
        //---------------------------------------------------------------------------
        //Randomly generate a direction as the orientation of the GNP
        if (!Get_initial_direction_mt(gnp_geo.orient_distrib_type, gnp_geo.ini_sita, gnp_geo.ini_pha, engine_orientation, dist_orientation, hybrid.rotation)) {
            hout << "Error in Generate_gnp_network_mt when calling Get_initial_direction_mt" << endl;
            return 0;
        }
        
        //---------------------------------------------------------------------------
        //Randomly generate a point inside the sample domain, this will be the displacement applied to the GNP, i.e,
        //its random location
        if(Get_seed_point_mt(excub, hybrid.center, engine_x, engine_y, engine_z, dist)==0) return 0;
        
        //---------------------------------------------------------------------------
        //Update flag of hybrid particle with the particle number (starts in 0)
        hybrid.flag = gnp_count;
        //Increase the counter of particles
        gnp_count++;
        //hout << "gnp_count=" << gnp_count << endl;
        
        //---------------------------------------------------------------------------
        //Discretizise the GNP
        vector<Point_3D> gnp_discrete;
        if (Discretize_gnp(hybrid, gnp_geo.discr_step_length, gnp_discrete)==0) {
            hout << "Error in Generate_gnp_network_mt when calling Discretize_gnp." << endl;
            return 0;
        }
        
        //---------------------------------------------------------------------------
        //Add to the vector of discretized gnps
        gnps_points.push_back(gnp_discrete);
        
        //Add generated volume fraction to the cumulative variables
        if (Calculate_generated_gnp_volume(gnp_geo, gvcub, hybrid, vol_sum, wt_sum)==0) {
            hout << "Error in Generate_gnp_network_mt when calling Calculate_generated_gnp_volume" << endl;
            return 0;
        }
        
        //Add the current particle to the vector of particles
        hybrid_praticles.push_back(hybrid);
    }
    
    carbon_vol = vol_sum;
    carbon_weight = wt_sum;
    
    if (particle_type != "Hybrid_particles") {
        //Print the number of GNPs when GNPs only or mixed fillers are generated
        if(gnp_geo.criterion == "wt") {
            hout << "The weight fraction of generated GNPs is: " << wt_sum/(geom_sample.volume*geom_sample.polymer_density) << endl;
            hout << ", the target weight fraction was " << gnp_geo.weight_fraction << endl << endl;
        } else if(gnp_geo.criterion == "vol") {
            hout << endl << "The volume fraction of generated GNPs was " << vol_sum/geom_sample.volume;
            hout << ", the target volume fraction was " << gnp_geo.volume_fraction << endl;
        }
        
        hout << "There were " << hybrid_praticles.size() << " GNPs generated." << endl;
    }
    
    return 1;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//Generate a network defined by points and connections
//Use the Mersenne Twister for the random number generation
int GenNetwork::Generate_cnt_network_threads_over_gnps_mt(const struct GNP_Geo &gnp_geo, const struct Geom_sample &geom_sample, const struct Nanotube_Geo &nanotube_geo, const struct Cutoff_dist &cutoffs, vector<vector<Point_3D> > &cnts_points, vector<vector<Point_3D> > &gnps_points, vector<GCH> &hybrid_praticles, vector<double> &cnts_radius)const
{
    //Generate random seed in terms of local time
    //unsigned int time_seed = 1453384844;
    //unsigned int time_seed = (unsigned int)time(NULL);
    //hout << "Time seed "<<time_seed<<endl;
    //srand(time_seed);
    //srand((unsigned int)time(NULL));
    
    //---------------------------------------------------------------------------
    //Set up the Mersenne Twisters used for the different variables
    // Use random_device to generate a seed for Mersenne twister engine.
    std::random_device rd;
    // Use Mersenne twister engine to generate pseudo-random numbers.
    //Generate differnet engines for different variables
    std::mt19937 engine_x(rd());
    std::mt19937 engine_y(rd());
    std::mt19937 engine_z(rd());
    std::mt19937 engine_pha(rd());
    std::mt19937 engine_sita(rd());
    std::mt19937 engine_rand(rd());
    
    // "Filter" MT's output to generate double values, uniformly distributed on the closed interval [0, 1].
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    //---------------------------------------------------------------------------
    double vol_sum_cnt = 0;  //the sum of volume of generated CNTs
    double wt_sum_cnt = 0;   //the sum of weight of generated CNTs
    double vol_sum_gnp = 0;  //the sum of volume of generated GNPs
    double wt_sum_gnp = 0;   //the sum of weight of generated GNPs
    double vol_sum = 0;  //the sum of volume of generated hybrid particles
    double wt_sum = 0;   //the sum of weight of generated hybrid particles
    int hybrid_count = 0; //to record the number of successfuly generated hybrid particles
    int cnt_reject_count = 0; //to record the number of CNTs that were deleted due to penetration
    int point_overlap_count = 0; //to record the number of times that a point had to be relocated
    int point_overlap_count_unique = 0; //to record the number of points that were overlapping other points
    
    //---------------------------------------------------------------------------
    //Vectors for handling CNT penetration
    //global_coordinates[i][0] stores the CNT number of global point i
    //global_coordinates[i][1] stores the local point number of global point i
    vector<vector<int> > global_coordinates;
    //sectioned_domain[i] contains all the points in sub-region i, a negative number indicates the discretized GNP
    //Sub-region i is an overlapping subregion to check for penetrations
    vector<vector<long int> > sectioned_domain;
    //n_subregions[0] is the number of subregions along x
    //n_subregions[1] is the number of subregions along y
    //n_subregions[2] is the number of subregions along z
    vector<int> n_subregions;
    //Define cutoff for overlapping
    double overlap_max_cutoff = 2*nanotube_geo.rad_max + cutoffs.van_der_Waals_dist;
    //Initialize the vector sub-regions
    Initialize_subregions(geom_sample, n_subregions, sectioned_domain);
    //This flag will be used to skip overlapping functions
    //1 = non-penetrating model
    //0 = penetrating model
    int penetrating_model_flag = 0;
    
    //---------------------------------------------------------------------------
    //Generate cuboids that represent the extended domain and the composite domain
    //To calculate the effective portion (length) which falls into the given region (RVE)
    struct cuboid gvcub;					//generate a cuboid to represent the composite domain
    gvcub.poi_min = geom_sample.origin;
    gvcub.len_x = geom_sample.len_x;
    gvcub.wid_y = geom_sample.wid_y;
    gvcub.hei_z = geom_sample.hei_z;
    gvcub.volume = geom_sample.volume;
    struct cuboid excub;					//generate a cuboid to represent the extended domain
    excub.poi_min = geom_sample.ex_origin;
    excub.len_x = geom_sample.ex_len;
    excub.wid_y = geom_sample.ey_wid;
    excub.hei_z = geom_sample.ez_hei;
    excub.volume = excub.len_x*excub.wid_y*excub.hei_z;
    
    //---------------------------------------------------------------------------
    //Vectors for handling CNT-CNT and CNT-GNP penetration within the current hybrid
    //global_coordinates[i][0] stores the CNT number of global point i, a negative number indicates the discretized GNP
    //global_coordinates[i][1] stores the local point number of global point i
    //These coordinates are only used for the current hybrid, after a hybrid is succesfully generated, the variable should be cleared
    vector<vector<int> > global_coord_hybrid;
    //sectioned_domain[i] contains all the points in sub-region i
    //Sub-region i is an overlapping subregion to check for penetrations
    //These subregions are only used for the current hybrid, after a hybrid is succesfully generated, the valiable should be cleared
    vector<vector<long int> > sect_domain_hybrid;
    //Initialize the vector sub-regions
    Initialize_subregions(geom_sample, n_subregions, sect_domain_hybrid);
    
    for (int i = 0; i < (int)hybrid_praticles.size(); i++) {
        //hout << "Hybrid " << i << endl;
        //---------------------------------------------------------------------------
        //Randomly generate a length of a CNT
        double cnt_length;
        if(Get_random_value_mt(nanotube_geo.len_distrib_type, engine_rand, dist, nanotube_geo.len_min, nanotube_geo.len_max, cnt_length)==0) return 0;
        
        //---------------------------------------------------------------------------
        //Randomly generate a radius of a CNT
        double cnt_rad;
        if(Get_random_value_mt(nanotube_geo.rad_distrib_type, engine_rand, dist, nanotube_geo.rad_min, nanotube_geo.rad_max, cnt_rad)==0) return 0;
        
        //---------------------------------------------------------------------------
        //Calculate the number of CNTs per side on the GNP
        //and check if the calculated number of CNTs is below the maximum number of CNTs that can be palced without penetration
        //If more CNTs are needed then terminate the function
        int num_seeds;         //the number of CNTs per side on a GNP
        if (Calculate_number_of_CNTs_on_GNP_surface(nanotube_geo, gnp_geo, hybrid_praticles[i].gnp, cnt_rad, cnt_length, num_seeds)==0) return 0;
        //hout << "num_seeds = " << num_seeds << endl;
        
        //---------------------------------------------------------------------------
        //The increased volume of each segement (growth step) of nanotube (Here the overlapping volume is ignored)
        const double step_vol_para = PI*cnt_rad*cnt_rad;
        //---------------------------------------------------------------------------
        //The increased weight of each segement (growth step) of nanotube (If the different radii of nanotube are considered, the linear_density may be different in every nanotube)
        const double step_wei_para = nanotube_geo.linear_density;
        
        //Vector to store all CNTs for the current hybrd particle
        vector<vector<Point_3D> > particle;
        
        //Flag to determine if a particle was succesfully created
        int particle_success = 0;
        
        //Check if CNTs will grow parallel or independent
        if (gnp_geo.growth_type == "independent") {
            //Grow CNTs in the "top" surface
            if (!Grow_cnts_on_gnp_surface(geom_sample, excub, hybrid_praticles[i], nanotube_geo, cutoffs, cnts_points, gnps_points, particle, global_coordinates, sectioned_domain, global_coord_hybrid, sect_domain_hybrid, cnts_radius, n_subregions, cnt_rad, cnt_length, overlap_max_cutoff, penetrating_model_flag, num_seeds, point_overlap_count, point_overlap_count_unique, cnt_reject_count, engine_x, engine_y, engine_sita, engine_pha, dist)) {
                //
                hout << "Error while growing CNTs on top surface of GNP " << i << endl;
                return 0;
            }
            
            //Add the CNT numbers corresponding to the top CNTs
            int current_size = (int)cnts_points.size();
            for (int j = 0; j < num_seeds; j++) {
                hybrid_praticles[i].cnts_top.push_back(current_size + j);
            }
            
            //Prepare rotation for bottom CNTs
            //Calculate the rotation matrix for the bottom CNTs
            //First invert the z-axis
            MathMatrix rotation_bottom = Get_transformation_matrix(PI, 0.0);
            //The rotation for the bottom is the same as the top but with the inverted z-axis
            rotation_bottom = hybrid_praticles[i].rotation*rotation_bottom;
            //Keep a copy of the original rotation
            MathMatrix rotation = hybrid_praticles[i].rotation;
            //Update the rotation of the GNP
            hybrid_praticles[i].rotation = rotation_bottom;
            
            //Grow CNTs in the "bottom" surface
            if (!Grow_cnts_on_gnp_surface(geom_sample, excub, hybrid_praticles[i], nanotube_geo, cutoffs, cnts_points, gnps_points, particle, global_coordinates, sectioned_domain, global_coord_hybrid, sect_domain_hybrid, cnts_radius, n_subregions, cnt_rad, cnt_length, overlap_max_cutoff, penetrating_model_flag, num_seeds, point_overlap_count, point_overlap_count_unique, cnt_reject_count, engine_x, engine_y, engine_sita, engine_pha, dist)) {
                //
                hout << "Error while growing CNTs on bottom surface of GNP " << i << endl;
                return 0;
            }
            
            //Replace the GNP rotation by the original one
            hybrid_praticles[i].rotation = rotation;
            
            //Add the CNT numbers corresponding to the top CNTs
            current_size = (int)cnts_points.size();
            for (int j = 0; j < num_seeds; j++) {
                hybrid_praticles[i].cnts_bottom.push_back(current_size + num_seeds + j);
            }
            
        } else if (gnp_geo.growth_type == "parallel") {
            //
        } else {
            hout << "Error. Invalid growth type: " << gnp_geo.growth_type;
            hout << ". Growth type should be either parallel or independent." << endl;
            return 0;
        }
        
        //Add hybrid particle to global data structure
        if (Add_CNTs_to_global_structure(geom_sample, particle, cnts_points, global_coordinates, sectioned_domain, n_subregions, cnts_radius, cnt_rad, overlap_max_cutoff, penetrating_model_flag)==0) return 0;
        
        //Add generated volume fraction of CNTs to the cumulative variables
        if (Calculate_generated_volume(gnp_geo, gvcub, hybrid_praticles[i], particle, step_vol_para, step_wei_para, vol_sum_cnt, wt_sum_cnt, vol_sum_gnp, wt_sum_gnp, vol_sum, wt_sum)==0) return 0;
        
        //Increase the hybrid count
        hybrid_count++;
        
        //Check if the target of hybrid particles content has been reached
        if ((nanotube_geo.criterion == "vol"&&vol_sum >= nanotube_geo.real_volume)||
            (nanotube_geo.criterion == "wt"&&wt_sum >= nanotube_geo.real_weight) ) {
            //If the target content has been generated, break the loop
            break;
        }
    }

    
    if(nanotube_geo.criterion == "wt") {
        hout << "The weight fraction of generated CNTs is: " << wt_sum_cnt/(geom_sample.volume*geom_sample.polymer_density) << endl;
        hout << "The weight fraction of generated GNPs is: " << wt_sum_gnp/(geom_sample.volume*geom_sample.polymer_density) << endl;
        hout << "The weight fraction of generated hybrid particles is: " << (wt_sum_gnp+wt_sum_cnt)/(geom_sample.volume*geom_sample.polymer_density) << ", the target weight fraction was " << nanotube_geo.weight_fraction+gnp_geo.weight_fraction << endl;
        hout << "The CNT/GNP mass ratio is " << wt_sum_cnt/wt_sum_gnp << ", the target mass ratio was " << gnp_geo.mass_ratio  << endl << endl;
    } else if(nanotube_geo.criterion == "vol") {
        hout << "The volume fraction of generated CNTs was " << vol_sum_cnt/geom_sample.volume << endl;
        hout << "The volume fraction of generated GNPs was " << vol_sum_gnp/geom_sample.volume << endl;
        hout << "The volume fraction of generated hybrid particles was " << (vol_sum_gnp+vol_sum_cnt)/geom_sample.volume << ", the target volume fraction was " << nanotube_geo.volume_fraction << endl; //The total volume fraction of hybrids is stored in the CNT variable
        
        hout << "The CNT/GNP mass ratio is " << (vol_sum_cnt*nanotube_geo.density)/(vol_sum_gnp*gnp_geo.density);
        //Compare with target mass ratio when necessary
        if (gnp_geo.mass_ratio > Zero) {
            hout << ", the target mass ratio was " << gnp_geo.mass_ratio  << endl << endl;
        } else {
            hout << endl << endl;
        }
    }
    
    //hout << "There were " << point_overlap_count_unique << " overlapping points and ";
    //hout << point_overlap_count << " overlaps, " << cnt_reject_count << " CNTs were rejected." << endl;
    
    hout << "There were " << hybrid_count << " hybrid particles generated. Initially " << hybrid_praticles.size() << " GNPS generated" << endl << endl;
    
    //Delete unused GNPs
    hybrid_praticles.erase(hybrid_praticles.begin()+hybrid_count,hybrid_praticles.end());
    gnps_points.erase(gnps_points.begin()+hybrid_count,gnps_points.end());
    
    hout << "Deleted additional GNPs, now there are " << hybrid_praticles.size() << " GNPs" << endl << endl;
    
    return 1;
}
//This fuction generates a GNP with a square base
//This fucntion also calculates the volume of the GNP
int GenNetwork::Generate_gnp(const struct GNP_Geo &gnp_geo, cuboid &gnp, mt19937 &engine_l, mt19937 &engine_t, uniform_real_distribution<double> &dist)const
{
    //Define size for the squared GNP surface
    gnp.len_x = gnp_geo.len_min + (gnp_geo.len_max - gnp_geo.len_min)*dist(engine_l);
    gnp.wid_y = gnp.len_x;
    
    //Thickness
    gnp.hei_z = gnp_geo.t_min + (gnp_geo.t_max - gnp_geo.t_min)*dist(engine_t);
    
    //Calculate volume
    gnp.volume = gnp.len_x*gnp.hei_z*gnp.wid_y;
    
    return 1;
}
//This function will loop until the maximum allowed number of attempts to handle GNP penetrations
int GenNetwork::Handle_gnp_penetrations(const struct cuboid &gvcub, const vector<GCH> &hybrid_particles, GCH &hybrid, Point_3D &gnp_poi)const
{
    int attempts = 0;
    while (attempts < MAX_ATTEMPTS) {
        //Check if there are any GNP penetrations and move the GNP if necessary
        if (!Check_penetration_and_move_gnp(gvcub, hybrid_particles, hybrid, gnp_poi)) {
            //If there are no penetrations, terminate the function
            return 1;
        }
        //After moving the GNP, we need to check if there are still penetrations
        
        //Increase the number of attempts for the next loop
        attempts++;
    }
    
    //If the maximum number of attepmts was reached without eliminating GNP penetrations, terminate the function with 0
    return 0;
}
//This function checks if a GNP is penetrating another GNP or GNPs
//1: penetration is detected
//0: no penetration
int GenNetwork::Check_penetration_and_move_gnp(const struct cuboid &gvcub, const vector<GCH> &hybrid_particles, GCH &hybrid, Point_3D &gnp_poi)const
{
    //Calculate radius of sphere that encloses current hybrid
    double sum = hybrid.gnp.len_x*hybrid.gnp.len_x + hybrid.gnp.wid_y*hybrid.gnp.wid_y + hybrid.gnp.hei_z*hybrid.gnp.hei_z;
    double radius = sqrt(sum)/2;
    
    //Temporary variables
    vector<int> indices;
    double sum2, radius2, separation;
    //Vectors to store the cutoffs and separations
    
    //Loop over other hybrid particles to find "penetrating" GNPs
    vector<double> cutoff, distances;
    for (int i = 0; i < (int)hybrid_particles.size(); i++) {
        //Calculate radius of sphere that encloses hybrid particle i
        sum2 = hybrid_particles[i].gnp.len_x*hybrid_particles[i].gnp.len_x + hybrid_particles[i].gnp.wid_y*hybrid_particles[i].gnp.wid_y + hybrid_particles[i].gnp.hei_z*hybrid_particles[i].gnp.hei_z;
        radius2 = sqrt(sum2)/2;
        
        //Calculate distance from center to center of GNPs
        separation = hybrid.center.distance_to(hybrid_particles[i].center);
        
        //If the distance between centers is less than the sum of radii, there is penetration of GNPs
        if (separation < radius + radius2) {
            cutoff.push_back(radius + radius2);
            distances.push_back(separation);
            indices.push_back(i);
        }
    }
    
    //If the cutoff vector (or distances vector) has a nonzero size, there are penetrating GNPs
    if (cutoff.size()) {
        //
        //hout << "Penetrating GNPs: " << cutoff.size() << endl;
        //hout << "Previous location: (" << hybrid.center.x << ", " << hybrid.center.y << ", " << hybrid.center.z << ")" << endl;
        
        //Point to store the new location of the centerpoint of the GNP
        Point_3D new_location;
        //Calculate the new location of the center point of the GNP
        if (Calculate_new_gnp_location(hybrid_particles, cutoff, distances, indices, new_location)==0) return 0;
        
        //Calculate the displacement vector of the GNP
        Point_3D displacement;
        displacement = new_location - hybrid.center;
        
        //Add the displacement to the gnp_poi
        gnp_poi = gnp_poi + displacement;
        
        //Update center point of GNP
        hybrid.center = new_location;
        
        //
        //hout << "New location: (" << hybrid.center.x << ", " << hybrid.center.y << ", " << hybrid.center.z << ")" << endl;
        
        //If it reached this point is beacause there were penetrations
        return 1;
    }
    
    return 0;
}
//Handle penetrations of GNPs
int GenNetwork::Calculate_new_gnp_location(const vector<GCH> &hybrid_particles, const vector<double> &cutoff, const vector<double> &distances, const vector<int> &indices, Point_3D &new_location)const
{
    //Some vectors need to be created in order to use the same functions as for CNT penetration
    
    //Create artificial "affected_points" vector. This vector stores all the centers of hybrid particles that are penetrated
    vector<Point_3D> affected_points;
    
    //Add elements to artificial vectors
    for (int i = 0; i < (int)indices.size(); i++) {
        int j = indices[i];
        //Add elements to the "affected_points" vector
        affected_points.push_back(hybrid_particles[j].center);
    }
    
    //Call the corresponding function depending on the number of overlaps
    if (indices.size() == 1) {
        One_overlapping_point(cutoff, distances, affected_points, new_location);
    } else if (indices.size() == 2) {
        Two_overlapping_points(cutoff, affected_points, new_location);
    } else {
        Three_or_more_overlapping_points(cutoff, distances, affected_points, new_location);
    }
    
    return 1;
}
//This function determines if at least one of the corners of the GNP are inside the composite domain (return 1) or outside (return 0)
int GenNetwork::GNP_inside_composite(const struct cuboid &gvcub, const GCH &hybrid)const
{    
    //First calculate the lower left corner
    //By doing this, it is assummed the center of the GNP is the origin (0,0,0)
    Point_3D corner( -hybrid.gnp.len_x/2, -hybrid.gnp.wid_y/2, -hybrid.gnp.hei_z/2);
    
    //Loop over the eight possible corners of the cube
    for(int i=0; i<2; i++)
        for(int j=0; j<2; j++)
            for(int k=0; k<2; k++) {
                
                //If i (j,k) is zero, then add nothing to the x (y,z) coordinate
                //If i (j,k) is one, then add the length on direction x (y,z) to the x (y,z) coordinate
                Point_3D adjust(((double)i)*hybrid.gnp.len_x, ((double)j)*hybrid.gnp.wid_y, ((double)k)*hybrid.gnp.hei_z);
                
                //Add the center and the "adjustment" so that the loop calculates the eight coordinates 
                adjust = corner + adjust;
                
                //Map to global coordinates
                adjust = adjust.rotation(hybrid.rotation, hybrid.center);
                
                if (Judge_RVE_including_point(gvcub, adjust)) {
                    //When the first corner is found, terminate the function and consider the GNP to be in a proper location
                    return 1;
                }
            }
    
    //If the loop finished, that menas that all corners of the GNP are outside the composite domain
    return 0;
}
//Given a CNT geometry and a GNP geometry, this fuction calculates the number of CNTs necesssary to keep the
//GNP to CNT weight ratio equal to 1
//The function also calculates the maximum possible number of CNTs that can be accomodated on a GNP surface without CNT penetration
//If more CNTs than those possible are required to keep the weight ratio equal to 1, then the funtion terminates
//with 0, this terminating the generetion process
int GenNetwork::Calculate_number_of_CNTs_on_GNP_surface(const struct Nanotube_Geo &nanotube_geo, const struct GNP_Geo &gnp_geo, const cuboid &gnp, const double &cnt_rad, const double &cnt_length, int &ns)const
{
    //Calculate the GNP area
    double GNP_area = gnp.len_x*gnp.wid_y;
    
    //Calculate the GNP volume
    double volume = GNP_area*gnp.hei_z;
    
    //The maximum nuber of CNTs that can be placed in a GNP surface is just the GNP surface area divided
    //by the cross-sectional area of the CNT
    double CNT_area = cnt_rad*cnt_rad*PI;
    int max_CNT = (int)(GNP_area/CNT_area);
    
    //Check if mass ratio or CNTs per micron
    if (gnp_geo.mass_ratio > Zero) {
        
        //Use mass ratio
        //Calculate the number of CNTs on the GNP surface that result in the input CNT to GNP mass ratio
        ns = (int)round(gnp_geo.mass_ratio*gnp_geo.density*volume/(2*nanotube_geo.density*cnt_length*CNT_area));
        
    }
    else {
        //Use CNTs per micron
        ns = -gnp_geo.mass_ratio*gnp.len_x*gnp.wid_y;
        
    }
    
    //hout << "ns = " << ns << endl;
    
    if (ns < max_CNT) {
        return 1;
    } else {
        hout << "The maximum number of CNTs that can be placed on a GNP surface without them penetrating eachother is " << max_CNT << '.' << endl;
        hout << "With the current CNT and GNP geometries, the number of CNTs required for a CNT to GNP weight ratio equal to 1 is ";
        hout << ns << '.' << endl;
        return 0;
    }
}
//
int GenNetwork::Generate_cnt_seeds(const struct Nanotube_Geo &nanotube_geo, const cuboid &gnp, vector<Point_3D> &seeds, vector<double> &radii, const double &d_vdw, const int &n_cnts, const double &z_coord, mt19937 &engine_x, mt19937 &engine_y, mt19937 &engine_rand, uniform_real_distribution<double> &dist)
{
    //Initialize a point
    Point_3D point(0.0,0.0,z_coord);
    double cnt_rad;
    
    //Generate the necessary number of CNT seeds
    for (int i = 0 ; i < n_cnts; i++) {
        //Get radius
        cnt_rad = Get_random_value_mt(nanotube_geo.rad_distrib_type, engine_rand, dist, nanotube_geo.rad_min, nanotube_geo.rad_max, cnt_rad);
        //In plane points
        if (Get_seed_point_2d_mt(gnp, cnt_rad, point, engine_x, engine_y, dist)==0) return 0;
        //Add to the vector of radii
        radii.push_back(cnt_rad);
        //Check for overlapping of seeds
        while (Check_seed_overlapping(seeds, cnt_rad, d_vdw, point)) {
            if (Get_seed_point_2d_mt(gnp, cnt_rad, point, engine_x, engine_y, dist)==0) return 0;
        }
        //Add into the vector of seeds
        seeds.push_back(point);
    }
    
    return 1;
}
//---------------------------------------------------------------------------
//Randomly generate a CNT seed (intial point of a CNT) in the surface of the GNP
//Note that the circle with certer at point and radius cnt_rad has to lie inside the surface of the GNP
int GenNetwork::Get_seed_point_2d_mt(const struct cuboid &cub, const double &cnt_rad, Point_3D &point, mt19937 &engine_x, mt19937 &engine_y, uniform_real_distribution<double> &dist)const
{
    //Adding cnt_rad and subtracting 2*cnt_rad to len_x, will make the CNT seed to be entirely inside the GNP surface
    point.x = -cub.len_x/2 + cnt_rad + (cub.len_x - 2*cnt_rad)*dist(engine_x);
    
    //Adding cnt_rad and subtracting 2*cnt_rad to wid_y, will make the CNT seed to be entirely inside the GNP surface
    point.y = -cub.wid_y/2 + cnt_rad + (cub.wid_y - 2*cnt_rad)*dist(engine_y);
    
    //By deafault the point is generated in the "top" surface, the rotation will later move it to the "bottom" surface if needed
    point.z = cub.hei_z/2;
    
    point.flag = 0; //0 denotes this point is the initial point of a CNT
    
    return 1;
}
int GenNetwork::Check_seed_overlapping(const vector<Point_3D> &seeds, const double &cnt_rad, const double &d_vdw, Point_3D &point)const
{
    //cutoff_p is used for the cutoff between two points (of different CNTs), distance is the actual distance
    //between those two points
    double cutoff_p, distance;
    
    //hout << "Check0 " << subregion_vec.size() << ' ';
    for (int i = 0; i < (int)seeds.size(); i++) {
        //hout << "Check4 ";
        cutoff_p = 2*cnt_rad + d_vdw;
        distance = point.distance_to(seeds[i]);
        //If the new point is overlapping a seed, then terminate the function and return 1
        if (distance < cutoff_p) {
            return 1;
        }
        //hout << "Check5 ";
    }
    //hout << "Check6 " << endl;
    return 0;
}
//This function generates all CNTs in one side of a GNP. CNTs grow independently of each other
int GenNetwork::Grow_cnts_on_gnp_surface(const struct Geom_sample &geom_sample, const struct cuboid &excub, const GCH &hybrid, const struct Nanotube_Geo &nanotube_geo, const struct Cutoff_dist &cutoffs, vector<vector<Point_3D> > &cnts_points, vector<vector<Point_3D> > &gnps_points, vector<vector<Point_3D> > &particle, const vector<vector<int> > &global_coordinates, const vector<vector<long int> > &sectioned_domain, vector<vector<int> > &global_coord_hybrid, vector<vector<long int> > &sect_domain_hybrid, vector<double> &cnts_radius, const vector<int> &n_subregions, const double &cnt_rad, const double  &cnt_length, const double &overlap_max_cutoff, const int &penetrating_model_flag, const int &ns, int &point_overlap_count, int &point_overlap_count_unique, int &cnt_reject_count, mt19937 &engine_x, mt19937 &engine_y, mt19937 &engine_sita, mt19937 &engine_pha, uniform_real_distribution<double> &dist)const
{
    //---------------------------------------------------------------------------
    //Initialize a point to be used in the rest of the function
    Point_3D cnt_poi(0.0,0.0,0.0);
    
    //---------------------------------------------------------------------------
    //Vectors for the CNT seeds, they will help in checking for seed overlapping
    vector<Point_3D> seeds;
    
    //Loop over the CNTs per side
    for (int i = 0; i < ns; i++) {
        //Vector to generate the current CNT
        vector<Point_3D> new_cnt;
        //Count the number of attemts to grow a CNT
        int attempts = 0;
        //Flag to determine that a CNT was grown successfully
        int success = 0;
        
        //While the maximum number of attempts has not been reached and CNT has not grown successfully, keep trying
        while (attempts < MAX_ATTEMPTS && !success) {
            
            //hout << "Grow0.00 ";
            if (Get_seed_point_2d_mt(hybrid.gnp, cnt_rad, cnt_poi, engine_x, engine_y, dist)==0) {
                hout << "Error in Grow_cnts_on_gnp_surface when calling Get_seed_point_2d_mt" <<endl;
                return 0;
            }
            
            //Apply rotation and displacement to the CNT seed
            cnt_poi = cnt_poi.rotation(hybrid.rotation, hybrid.center);
            
            //Check seed penetration
            //If seed is penetrating then, generate another seed (that is why the rest of the code is inside the if-statement)
            //hout << "Grow0.0 ";
            if (!Check_seed_overlapping(seeds, cnt_rad, cutoffs.van_der_Waals_dist, cnt_poi)) {
                //If not penetrating, then add current seed to the vector of seeds
                seeds.push_back(cnt_poi);
                //Store this seed point in the new nanotube vector
                new_cnt.push_back(cnt_poi);
                //hout << "Grow1.0 ";
                
                //Grow CNT
                //hout << "CNT growth start" << endl;
                if (Grow_single_cnt_on_gnp(geom_sample, excub, nanotube_geo, cutoffs, cnts_points, gnps_points, particle, global_coordinates, sectioned_domain, global_coord_hybrid, sect_domain_hybrid, new_cnt, cnts_radius, n_subregions, hybrid.rotation, cnt_rad, cnt_length, penetrating_model_flag, point_overlap_count, point_overlap_count_unique, cnt_reject_count, engine_sita, engine_pha, dist)) {
                    //If CNT was grown succesfully, set the flag to 1 to break the while loop
                    success = 1;
                    
                    //hout << "Grow2.1 ";
                    
                    //Add new CNT to the vectors for overlapping (global_coord_hybrid, sect_domain_hybrid)
                    if (penetrating_model_flag) {
                        if (Add_CNTs_to_hybrid_structure(geom_sample, particle, new_cnt, global_coord_hybrid, sect_domain_hybrid, n_subregions, overlap_max_cutoff)==0) {
                            hout << "Error in Grow_cnts_on_gnp_surface when calling Add_CNTs_to_hybrid_structure"<<endl;
                            return 0;
                        }
                    }
                    
                    //Add CNT to the vector of points for the current particle
                    particle.push_back(new_cnt);
                    //hout << "Grow2.2 ";
                } else {
                    //If the CNT was not grown successfully, then try again
                    //Increase the number of attempts
                    attempts++;
                    
                    //Remove the last CNT seed
                    seeds.pop_back();
                    
                    //Remove all generated points
                    new_cnt.clear();
                    
                    //hout << "Grow3.0 ";
                }
                //hout << "CNT growth end" << endl;
                
            } else {
                //If the CNT seed was overlapping, then try again
                //Increase the number of attempts
                attempts++;
            }
            //hout << "Grow0.2 ";
            
        }
        
        //If the CNT was not grown succesfully after the maximum number of attempts
        //then break the loop and create a new hybrid particle
        if ((attempts == MAX_ATTEMPTS) && !success) {
            hout << "Reached maximum number of attempts to grow a CNT"<<endl;
            return 0;
        }
        
    }
    
    return 1;
}
//This function generates all CNTs in one side of a GNP. CNTs grow parallel
int GenNetwork::Grow_CNTs_on_GNP_surface_parallel(const struct Geom_sample &geom_sample, const struct cuboid &excub, const struct cuboid &gnp, const struct Nanotube_Geo &nanotube_geo, const struct Cutoff_dist &cutoffs, vector<vector<Point_3D> > &cnts_points, vector<vector<Point_3D> > &gnps_points, vector<vector<Point_3D> > &particle, vector<Point_3D> &gnp_discrete, const vector<vector<int> > &global_coordinates, const vector<vector<long int> > &sectioned_domain, vector<vector<int> > &global_coord_hybrid, vector<vector<long int> > &sect_domain_hybrid, vector<double> &cnts_radius, const vector<int> &n_subregions, const MathMatrix &multiplier, const Point_3D &gnp_poi, const double &cnt_rad, const double  &cnt_length, const int &penetrating_model_flag, const int &ns, int &point_overlap_count, int &point_overlap_count_unique, int &cnt_reject_count, mt19937 &engine_x, mt19937 &engine_y, mt19937 &engine_sita, mt19937 &engine_pha, uniform_real_distribution<double> &dist)const
{
    //---------------------------------------------------------------------------
    //Initialize a point to be used in the rest of the function
    Point_3D cnt_poi(0.0,0.0,0.0);
    
    //---------------------------------------------------------------------------
    //Vectors for the CNT seeds, they will help in checking for seed overlapping
    vector<Point_3D> seeds;
    //Vector to generate the initial CNT
    vector<Point_3D> initial_cnt;
    
    //Generate initial seed
    cnt_poi.z = gnp.hei_z/2;
    if (Get_seed_point_2d_mt(gnp, cnt_rad, cnt_poi, engine_x, engine_y, dist)==0) return 0;
    //Apply rotation and displacement to the CNT seed
    cnt_poi = cnt_poi.rotation(multiplier, gnp_poi);
    //Add current seed to the vector of seeds
    seeds.push_back(cnt_poi);
    //Store this seed point in the initial nanotube vector
    initial_cnt.push_back(cnt_poi);
    
    //Grow intial CNT
    //hout << "CNT growth start" << endl;
    if (Grow_single_cnt_on_gnp(geom_sample, excub, nanotube_geo, cutoffs, cnts_points, gnps_points, particle, global_coordinates, sectioned_domain, global_coord_hybrid, sect_domain_hybrid, initial_cnt, cnts_radius, n_subregions, multiplier, cnt_rad, cnt_length, penetrating_model_flag, point_overlap_count, point_overlap_count_unique, cnt_reject_count, engine_sita, engine_pha, dist)==0) return 0;
    //hout << "CNT growth end" << endl;
    //Add the initial CNT to the vector of points for the current particle
    particle.push_back(initial_cnt);
    
    
    //Loop over the CNTs per side. Starts at i=1 because the first CNT was already generated
    for (int i = 1; i < ns; i++) {
        //Vector to generate the current CNT
        vector<Point_3D> new_cnt;
        //Count the number of attemts to grow a CNT
        int attempts = 0;
        //Flag to determine that a CNT was grown successfully
        int success = 0;
        
        //While the maximum number of attempts has not been reached and CNT has not grown successfully, keep trying
        while (attempts < MAX_ATTEMPTS && !success) {
            //Create a CNT seed in the top surface
            cnt_poi.z = gnp.hei_z/2;
            if (Get_seed_point_2d_mt(gnp, cnt_rad, cnt_poi, engine_x, engine_y, dist)==0) return 0;
            
            //Apply rotation and displacement to the CNT seed
            cnt_poi = cnt_poi.rotation(multiplier, gnp_poi);
            
            //Check seed penetration
            //If seed is penetrating then, generate another seed
            //hout << "Grow0.0 ";
            if (!Check_seed_overlapping(seeds, cnt_rad, cutoffs.van_der_Waals_dist, cnt_poi)) {
                //If not penetrating, then add current seed to the vector of seeds
                seeds.push_back(cnt_poi);
                //Store this seed point in the new nanotube vector
                new_cnt.push_back(cnt_poi);
                //hout << "Grow1.0 ";
                
                //Copy CNT
                if(Copy_CNT(initial_cnt, new_cnt)) {
                    //Update the success flag
                    success = 1;
                    //Add CNT to the vector of points for the current particle
                    particle.push_back(new_cnt);
                } else {
                    return 0;
                }
                
            } else {
                //If the CNT seed was overlapping, then try again
                //Increase the number of attempts
                attempts++;
            }
            //hout << "Grow0.2 ";
        }
        
        //If the CNT was not grown succesfully after the maximum number of attempts
        //then break the loop and create a new hybrid particle
        if ((attempts == MAX_ATTEMPTS) && !success) {
            return 0;
        }
        
    }
    
    return 1;
}
//This function generates a CNT by displacing an initial CNT
int GenNetwork::Copy_CNT(const vector<Point_3D> &initial_cnt, vector<Point_3D> &new_cnt)const
{
    //Calculate the displacement of the new CNT
    Point_3D displacement = new_cnt.front() - initial_cnt.front();
    
    //This point will be the new point
    Point_3D new_location;
    
    //Loop over the points of the initial CNT to generate the new CNT
    for (int i = 1; i < (int)initial_cnt.size(); i++) {
        //Calculate new location
        new_location = initial_cnt[i];
        new_location = new_location + displacement;
        
        //Add new point to new CNT
        new_cnt.push_back(new_location);
    }
    
    return 1;
}
//This function will be used to "grow" the CNTs in the surface of a GNP
//1: successfully grown CNT
//0: could not solve penetration of CNT
int GenNetwork::Grow_single_cnt_on_gnp(const struct Geom_sample &geom_sample, const struct cuboid &excub, const struct Nanotube_Geo &nanotube_geo, const struct Cutoff_dist &cutoffs, vector<vector<Point_3D> > &cnts_points, vector<vector<Point_3D> > &gnps_points, vector<vector<Point_3D> > &particle, const vector<vector<int> > &global_coordinates, const vector<vector<long int> > &sectioned_domain, const vector<vector<int> > &global_coord_hybrid, const vector<vector<long int> > &sect_domain_hybrid, vector<Point_3D> &new_cnt, vector<double> &cnts_radius, const vector<int> &n_subregions, const MathMatrix &seed_multiplier, const double &cnt_rad, const double  &cnt_length, const int &penetrating_model_flag, int &point_overlap_count, int &point_overlap_count_unique, int &cnt_reject_count, mt19937 &engine_sita, mt19937 &engine_pha, uniform_real_distribution<double> &dist)const
{
    //Initialize the multiplier matrix with the initial rotation
    MathMatrix multiplier = seed_multiplier;
    //CNT seed
    Point_3D cnt_poi = new_cnt.front();
    
    //variables for the angle orientations
    double cnt_sita, cnt_pha;
    
    //---------------------------------------------------------------------------
    //Calculate the total number of growth steps for a CNT
    int step_num = (int)(cnt_length/nanotube_geo.step_length) + 1;
    
    //---------------------------------------------------------------------------
    for(int i=0; i<step_num; i++)
    {
        //hout << "Point#" << i<<" of "<<step_num-1<<' '<<endl;
        //Randomly generate a direction in the spherical coordinates
        //To have the positive Z-axis to be a central axis
        //Then, the radial angle, sita, obeys a normal distribution (sita \in fabs[(-omega,+omega)]) and the zonal angle, pha, obeys a uniform distribution (pha \in (0,2PI))
        if(Get_normal_direction_mt(nanotube_geo.angle_max, cnt_sita, cnt_pha, engine_sita, engine_pha, dist)==0) return 0;
        
        //To calculate the new multiplier for transformation of coordinates
        multiplier = multiplier*Get_transformation_matrix(cnt_sita, cnt_pha);
        
        //To calculate the coordinates of the new CNT point (transformation of coordinates)
        cnt_poi = cnt_poi + Get_new_point(multiplier, nanotube_geo.step_length);
        cnt_poi.flag = 1;							//1 means that point is not the intial point
        
        //---------------------------------------------------------------------------
        //If the new CNT point grows out of the extended domain, terminate generation of the CNT
        bool touch_end = false;
        //If both points are outside, do not calculate the intersection with boundary
        //Only calculate the intersection when cnt_poi is outside and the previous point, new_cnt.back() is inside
        if(Judge_RVE_including_point(excub, cnt_poi)==0)
        {
            //Break the for-loop
            touch_end = true;
        }
        
        //---------------------------------------------------------------------------
        //CHANGES FOR PENETRATING MODEL GO HERE
        //---------------------------------------------------------------------------
        new_cnt.push_back(cnt_poi);							//store a new point
        
        if (touch_end) {
            //When touch_end is true that means a CNT reached the boundaries of the extended volume
            //Terminate the function and return 1 to indicate the CNT grew succesfully
            return 1;
        }
        
    }
    
    //The CNT grew the whole lenght without errors
    return 1;
}
//This functions adds to the global structure the CNTs generated for a hybrid particle
int GenNetwork::Add_CNTs_to_hybrid_structure(const struct Geom_sample &geom_sample, const vector<vector<Point_3D> > &particle, vector<Point_3D> &new_cnt, vector<vector<int> > &global_coord_hybrid, vector<vector<long int> > &sect_domain_hybrid, const vector<int> &n_subregions, const double &overlap_max_cutoff)const
{
    int CNT_number = (int)particle.size();
    //Loop over the generated CNTs, i.e. over particle.size()
    for (int j = 0; j < (int)new_cnt.size(); j++) {
        //Variables needed for updating global_coordinates
        vector<int> empty;

        //Add global coordinate
        global_coord_hybrid.push_back(empty);
        global_coord_hybrid.back().push_back(CNT_number);
        global_coord_hybrid.back().push_back(j);
        //Add point to an overlapping region in the vector sectioned_domain
        Add_to_overlapping_regions(geom_sample, overlap_max_cutoff, new_cnt[j], (long int)global_coord_hybrid.size()-1, n_subregions, sect_domain_hybrid);
        
    }
    return 1;
}
//This functions adds to the global structure the CNTs generated for a hybrid particle
int GenNetwork::Add_CNTs_to_global_structure(const struct Geom_sample &geom_sample, const vector<vector<Point_3D> > &particle, vector<vector<Point_3D> > &cnts_points, vector<vector<int> > &global_coordinates, vector<vector<long int> > &sectioned_domain, vector<int> &n_subregions, vector<double> &cnts_radius, double &cnt_rad, double &overlap_max_cutoff, int &penetrating_model_flag)const
{
    //Loop over the generated CNTs, i.e. over particle.size()
    for (int j = 0; j < (int)particle.size(); j++) {
        //---------------------------------------------------------------------------
        //Store the CNT points
        
        //Perform these operations when the non-overlapping model is used
        if (penetrating_model_flag) {
            //Variables needed for updating global_coordinates
            vector<int> empty;
            int CNT_number = (int)cnts_points.size();
            for(int i=0; i<(int)particle[j].size(); i++) {
                //Add global coordinate
                global_coordinates.push_back(empty);
                global_coordinates.back().push_back(CNT_number);
                global_coordinates.back().push_back(i);
                //Add point to an overlapping region in the vector sectioned_domain
                Add_to_overlapping_regions(geom_sample, overlap_max_cutoff, particle[j][i], (long int)global_coordinates.size()-1, n_subregions, sectioned_domain);
                
            }
        }
        
        //Add the current CNT and CNT radius to the global vectors
        cnts_points.push_back(particle[j]);     //to store the points of a CNT
        cnts_radius.push_back(cnt_rad);         //to store the radius of a CNT
    }
    
    return 1;
}
//This function calculates the generated volume and adds it to the global variables
int GenNetwork::Calculate_generated_volume(const struct GNP_Geo &gnp_geo, const struct cuboid &gvcub, const GCH &hybrid, const vector<vector<Point_3D> > &particle, const double &step_vol_para, const double &step_wei_para, double &vol_sum_cnt, double &wt_sum_cnt, double &vol_sum_gnp, double &wt_sum_gnp, double &vol_sum_tot, double &wt_sum_tot)const
{
    
    //---------------------------------------------------------------------------
    //Add the volume and weight corresponding to the CNTs
    
    //Variable to store the effective length
    double temp_length;
    //Variables to store the CNT volume and weight
    double cnt_vol = 0, cnt_wei = 0;
    for (int i = 0; i < (int)particle.size(); i++) {
        for (int j = 0; j < (int)particle[i].size()-1; j++) {
            temp_length = Effective_length_given_region(gvcub, particle[i][j],particle[i][j+1]);
            //double temp_length = new_cnt.back().distance_to(cnt_poi);
            if (temp_length > 0.0)
            {
                cnt_vol += temp_length*step_vol_para;		//add accumulation on the volume
                cnt_wei += temp_length*step_wei_para;		//add accumulation on the weight
            }
        }
    }
    //Add the volume and weight to the cummulative variables
    vol_sum_tot += cnt_vol;
    wt_sum_tot += cnt_wei;
    vol_sum_cnt += cnt_vol;
    wt_sum_cnt += cnt_wei;
    
    //---------------------------------------------------------------------------
    //Add the volume and weight corresponding to the GNP
    double gnp_vol = hybrid.gnp.volume;
    
    //Check if the center of the particle is outside the sample domain or close enough to the boundaries of the composite
    if (!Judge_RVE_including_point(gvcub, hybrid.center)|| Is_close_to_boundaries(gvcub, hybrid)) {
        //If the particle is close enough, approximate the volume of the GNP
        if (!Approximate_gnp_volume(gvcub, hybrid, gnp_vol)) {
            hout << "Error in Calculate_generated_volume when calling Approximate_gnp_volume." << endl;
            return 0;
        }
    }
    
    //Approximate the volume inside the composite
    vol_sum_tot += gnp_vol;                     //add accumulation on the volume
    wt_sum_tot += gnp_vol*gnp_geo.density;		//add accumulation on the weight
    vol_sum_gnp += gnp_vol;
    wt_sum_gnp += gnp_vol*gnp_geo.density;
    
    
    return 1;
}
//This function calculates the generated volume of a GNP and adds it to the global variables
int GenNetwork::Calculate_generated_gnp_volume(const struct GNP_Geo &gnp_geo, const struct cuboid &gvcub, const GCH &hybrid, double &vol_sum, double &wt_sum)const
{
    //---------------------------------------------------------------------------
    //Add the volume and weight corresponding to the GNP
    double gnp_vol = hybrid.gnp.volume;
    
    //Check if the center of the particle is close enough to the boundaries of the composite
    if (Is_close_to_boundaries(gvcub, hybrid)) {
        //If the particle is close enough, approximate the volume of the GNP
        if (!Approximate_gnp_volume(gvcub, hybrid, gnp_vol)) {
            hout << "Error in Calculate_generated_volume when calling Approximate_gnp_volume." << endl;
            return 0;
        }
    }
    
    //Approximate the volume inside the composite
    vol_sum += gnp_vol;                     //add accumulation on the volume
    wt_sum += gnp_vol*gnp_geo.density;		//add accumulation on the weight
    
    return 1;
}
//This function determines if a GNP is close enough to the boundaries
int GenNetwork::Is_close_to_boundaries(const struct cuboid &gvcub, const GCH &hybrid)const
{
    //Calculate half the diagonal of the GNP, i.e. the radius of the sphere that encloses the GNP
    double sum = hybrid.gnp.len_x*hybrid.gnp.len_x + hybrid.gnp.wid_y*hybrid.gnp.wid_y + hybrid.gnp.hei_z*hybrid.gnp.hei_z;
    double radius = sqrt(sum)/2;
    
    if (hybrid.center.x < gvcub.poi_min.x + radius || hybrid.center.x > gvcub.poi_min.x + gvcub.len_x - radius) {
        return 1;
    } else if (hybrid.center.y < gvcub.poi_min.y + radius || hybrid.center.y > gvcub.poi_min.y + gvcub.wid_y - radius) {
        return 1;
    } else if (hybrid.center.z < gvcub.poi_min.z + radius || hybrid.center.z > gvcub.poi_min.z + gvcub.hei_z - radius) {
        return 1;
    }
    
    //If none of the cases above, then the GNP is not close to the boundaries
    return 0;
    
}
//
int GenNetwork::Approximate_gnp_volume(const struct cuboid &gvcub, const GCH &hybrid, double &gnp_vol)const
{
    //---------------------------------------------------------------------------
    //Number of points per side to approximate volume
    int n_points = 20;
    
    //Variable to count the number of points inside the composite
    int points_in = 0;
    
    //These points determine the location of the n_points*n_points points that are used to approximate the volume of the GNP
    Point_3D step_x(hybrid.gnp.len_x/((double)n_points),0.0,0.0);
    Point_3D step_y(0.0,hybrid.gnp.wid_y/((double)n_points),0.0);
    
    //First calculate the lower left corner on the central plane of the GNP
    //It is assummed the center of the GNP is the origin (0,0,0), thus the center of the hybrid particle is the displacement
    //to be used in the function that maps to global coordinates
    Point_3D corner( -hybrid.gnp.len_x/2, -hybrid.gnp.wid_y/2, 0.0);
    
    //Loop over the points on the central plane
    for(int i=0; i<n_points; i++)
        for(int j=0; j<n_points; j++) {
            //A point that approximates the volume will be i steps in the x-direction and j steps in the y-direction
            Point_3D temp = corner + step_x*i + step_y*j;
            
            //Map to global coordinates
            temp = temp.rotation(hybrid.rotation, hybrid.center);
            
            if (Judge_RVE_including_point(gvcub, temp)) {
                //Increase the counter of points inside
                points_in++;
            }
        }
    
    //Approximate the volume inside
    //hout << "Total volume = " << gnp_vol;
    gnp_vol = hybrid.gnp.volume*((double)points_in/(n_points*n_points));
    //hout << ". Approximated volume = " << gnp_vol << "points_in="<<points_in<<", (total points=" <<n_points*n_points<<")."<<endl;
    return 1;
}
//This function takes a GNP and generates a discretized version.
//This dicretized version consists of a vector of points that approximates the six surfaces of the GNP with a square lattice.
//The distance between points is equal to the step size of the CNTs.
int GenNetwork::Discretize_gnp(const GCH &hybrid, const double &step, vector<Point_3D> &gnp_discrete)const
{
    
    //The surface is squared, so generate all points in a single line from -l/2 to l/2
    //where l is the side length of the squared surface
    double largest = hybrid.gnp.len_x/2, smallest = - largest;
    vector<double> step_line;
    
    //Initialize the current step with the first step, i.e. smallest
    double current_step = smallest;
    
    //Add steps while the current step is smaller than the largest step
    while (current_step < largest) {
        
        //Add the current step to the vector of steps
        step_line.push_back(current_step);
        
        //Increase the step
        current_step = current_step + step;
    }
    
    //Add the last step
    step_line.push_back(largest);
    
    
    //---------------------------------------------------------------------------
    //Bottom and top surface
    
    //The bottom and top surfaces are all combinatios of two steps in the step_line with +lz/2 and -lz/2
    for (int i = 0; i < (int)step_line.size(); i++) {
        
        for (int j = 0; j < (int)step_line.size(); j++) {
            
            //Point that will be added to the discretization vector
            Point_3D point_tmp(step_line[i], step_line[j], hybrid.gnp.hei_z/2);
            
            //Map to global coordinates
            point_tmp = point_tmp.rotation(hybrid.rotation, hybrid.center);
            
            //Assign same flag as hybrid
            point_tmp.flag = hybrid.flag;
            
            //Add the point to the discretization vector
            gnp_discrete.push_back(point_tmp);
            
            //Generate the same point with at the opposite z-boundary
            point_tmp.x = step_line[i];
            point_tmp.y = step_line[j];
            point_tmp.z = -hybrid.gnp.hei_z/2;
            
            //Map to global coordinates
            point_tmp = point_tmp.rotation(hybrid.rotation, hybrid.center);
            
            //Assign same flag as hybrid
            point_tmp.flag = hybrid.flag;
            
            //Add the point to the discretization vector
            gnp_discrete.push_back(point_tmp);
        }
    }
    
    //---------------------------------------------------------------------------
    //Lateral surfaces
    
    //Reset largest and smallest
    largest = hybrid.gnp.hei_z/2; smallest = -largest;
    
    //Step vector for the lateral surfaces, they will be added in "layers"
    vector<double> step_layer;
    
    //Initialize the current step with the first step, i.e. smallest
    current_step = smallest;
    
    //Add steps while the current step is smaller than the largest step
    while (current_step < largest) {
        
        //Add the current step to the vector of steps
        step_layer.push_back(current_step);
        
        //Increase the step
        current_step = current_step + step;
    }
    
    //Add the last step
    step_layer.push_back(largest);
    
    //Scan all steps except the first and last since those are the squared sufaces and
    //they were already discretized
    for (int k = 1; k < (int)step_layer.size()-1; k++) {
        
        //Scan all possible combinations of the step_line vector
        for (int i = 0; i < (int)step_line.size(); i++) {
            
            for (int j = 0; j < (int)step_line.size(); j++) {
                
                //A point is in a lateral surface is i=0 or i=step_line.size()-1 j=0 or j=step_line.size()-1
                if (i == 0 || i == (int)step_line.size()-1 || j == 0 || j == (int)step_line.size()-1) {
                    
                    //Generate the lateral point
                    Point_3D point_tmp(step_line[i], step_line[j], step_layer[k]);
                    
                    //Map to global coordinates
                    point_tmp = point_tmp.rotation(hybrid.rotation, hybrid.center);
                    
                    //Assign same flag as hybrid
                    point_tmp.flag = hybrid.flag;
                    
                    //Add the point to the discretization vector
                    gnp_discrete.push_back(point_tmp);
                }
            }
        }
    }
    
    return 1;
}
//Check if the current CNT is penetrating another CNT, i.e. is the new point is overlapping other point
//1: a) No penetration
//   b) No need to check for penetration (point is in boundary layer or there are no other points in the same sub-region)
//   c) There was penetration but it was succesfully resolved
//0: There was penetration but could not be resolved
int GenNetwork::Check_penetration_in_gch(const struct Geom_sample &geom_sample, const struct Nanotube_Geo &nanotube_geo, const vector<vector<Point_3D> > &cnts, const vector<vector<Point_3D> > &gnps_points, const vector<vector<Point_3D> > &particle, const vector<Point_3D> &gnp_discrete, const vector<vector<int> > &global_coordinates, const vector<vector<long int> > &sectioned_domain, const vector<vector<int> > &global_coord_hybrid, const vector<vector<long int> > &sect_domain_hybrid, const vector<double> &radii, const vector<Point_3D> &cnt_new, const vector<int> &n_subregions, const double &cnt_rad, const double &d_vdw, int &point_overlap_count, int &point_overlap_count_unique, Point_3D &point)const
{
    //Get the sub-region the point belongs to
    int subregion = Get_subregion(geom_sample, n_subregions, point);
    
    //If the sub-region is -1, then the point is in te boundary layer, so there is no need to check penetration
    if (subregion == -1) {
        return 1;
    }
    
    //This vector will store the coordintes of the points that the input "point" is penetrating
    vector<Point_3D> affected_points;
    //This vector stores the distance at which the two points should be
    vector<double> cutoffs_p;
    //This vector stores the distance at which the two points actually are
    vector<double> distances;
    //Need to keep the count of the number of attempts outside the for-loop
    int attempts;
    //I move the point up to max_attempts times. If there is still penetration then I delete it
    //Create a temporary 2D vector of points to keep consitency with the functions
    vector<vector<Point_3D> > gnps_points_tmp;
    gnps_points_tmp.push_back(gnp_discrete);
    for (attempts = 0; attempts <= MAX_ATTEMPTS; attempts++) {
        
        //Find penetrating points among the successfully generated hybrids
        //hout<<"Penetration with the successfully generated hybrids. sectioned_domain["<<subregion<<"].size()="<<sectioned_domain[subregion].size()<< endl;
        if (!Get_penetrating_points(cnts, gnps_points, global_coordinates, sectioned_domain[subregion], radii, cnt_rad, d_vdw, 0, point, affected_points, cutoffs_p, distances)) {
            hout << "Error in Check_penetration_in_gch while calling Get_penetrating_points. The current CNT will be discarded." << endl;
            return 0;
        }
        //int tmp = (int)affected_points.size();
        //hout << "Overlapping points with structure: " << tmp << endl;
        
        //hout << "Penetration with current hybrid. sect_domain_hybrid["<<subregion<<"].size()="<<sect_domain_hybrid[subregion].size()<< endl;
        //Find penetrating points among the current hybrid
        if (!Get_penetrating_points(particle, gnps_points_tmp, global_coord_hybrid, sect_domain_hybrid[subregion], radii, cnt_rad, d_vdw, 1, point, affected_points, cutoffs_p, distances)) {
            hout << "Error in Check_penetration_in_gch while calling Get_penetrating_points. The current CNT will be discarded." << endl;
            return 0;
        }
        //hout << "Overlapping points within hybrid: " << (int)affected_points.size() - tmp << endl;
        
        //--------------------------------------------------------------------------------------------
        //Check if there are any penetrating points
        if (affected_points.size()) {
            //Update the counter of overlaps
            point_overlap_count++;
            
            //Update the counter of overlapping points only when an overlapping point was found the first time
            //i.e. attempts = 0
            if (!attempts) {
                point_overlap_count_unique++;
            }
            
            //If this is the last iteration and there are still affected points then the point could not be accommodated
            if (attempts == MAX_ATTEMPTS) {
                //hout << "Deleted CNT number " << cnts.size() << " of size " << cnt_new.size();
                //hout << " (reached maximum number of attempts for relocation)" << endl;//*/
                return 0;
            }
            
            //Find the new point
            //hout << "Point " << cnt_new.size() << " in CNT " << particle.size() << " in hybrid " << gnps_points.size() << " is overlapping." <<endl;
            //hout << "affected_points="<<affected_points.size()<<" cutoffs_p="<<cutoffs_p.size()<<" distances="<<distances.size()<<endl;
            //hout << "Moved a point from initial position (" << point.x << ", " << point.y << ", " << point.z << ")." << endl;
            Move_point(geom_sample, nanotube_geo, cnt_new, point, cutoffs_p, distances, affected_points);
            //hout << "Moved a point to final position (" << point.x << ", " << point.y << ", " << point.z << ")." << endl;
            
            //Check that the new point is within the permited orientation repect to the previous segment
            if (!Check_segment_orientation(point, cnt_new)) {
                //hout << "Deleted CNT number " << cnts.size() << " of size " << cnt_new.size();
                //hout << " (the point is not in a valid orientation)" << endl;//*/
                //When not in a valid position it cannot be moved again so a new CNT is needed
                return 0;
            }
            
            //Need to update point sub-region as it could be relocated to a new sub-region
            subregion = Get_subregion(geom_sample, n_subregions, point);
            //Check if after moving the point it is now in the boundary layer
            if (subregion == -1) {
                //If the point is now in the boundary layer, terminate the function
                //there is no need to continue checking
                return 1;
            }
            
            //Need to clear the vectors affected_points, contact_coordinates and temporal_contacts so they are used again with the new point
            affected_points.clear();
            cutoffs_p.clear();
            distances.clear();
            
        } else {
            //if the size of affected_points is zero, then terminate the function
            return 1;
        }
    }
    
    return 1;
}
//This functions iterates over a sub-region and determines if there are any penetrating points
//If there are penetrating points, they are stored in the vector affected_points
int GenNetwork::Get_penetrating_points(const vector<vector<Point_3D> > &cnts, const vector<vector<Point_3D> > &gnps_points,  const vector<vector<int> > &global_coordinates, const vector<long int> &subregion_vec, const vector<double> &radii, const double &cnt_rad, const double &d_vdw, const int &hybrid_flag, Point_3D &point, vector<Point_3D> &affected_points, vector<double> &cutoffs_p, vector<double> &distances)const
{
    //They are just intermediate variables and I only use them to make the code more readable
    long int coord2;
    int P2, CNT2, GNP2;
    //cutoff_p is used for the cutoff between two points (of different CNTs), distance is the actual distance
    //between those two points
    double cutoff_p;
    
    //hout << "Check0 " << subregion_vec.size() << ' ';
    //Get the penetrating points related to the successfully generated hybrids
    //int n_CNT=0, n_GNP=0;
    for (int i = 0; i < (int)subregion_vec.size(); i++) {
        //hout << "Check1 " ;
        coord2 = subregion_vec[i];
        //hout << "Check2 coord2="<<coord2<<" global_coordinates.s="<<global_coordinates.size()<<' ';
        //Check if the first coordinate is positive (CNT number) or negative (GNP number)
        if (global_coordinates[coord2][0] >= 0) {
            CNT2 = global_coordinates[coord2][0];
            P2 = global_coordinates[coord2][1];
            //hout << "Check4 CNT2="<<CNT2<<" P2="<<P2<<" cnts[CNT2].size()="<<cnts[CNT2].size();
            //hout <<" ("<<cnts[CNT2][P2].x<<", "<<cnts[CNT2][P2].y<<", "<<cnts[CNT2][P2].z<<") "<<endl;
            if (hybrid_flag) {
                //If this is a hybrid, then all CNTs have the same radius
                cutoff_p = 2.0*cnt_rad + d_vdw;
            } else {
                //If this is the successfully generated structure, then the second radius might be different
                cutoff_p = cnt_rad + radii[CNT2] + d_vdw;
            }
            //hout << "Check5.1 ";
            //Chek if the point is below the cutoff; if so, add it to the corresponding vectors
            if (Is_below_cutoff(cnts[CNT2][P2], point, cutoff_p, affected_points, cutoffs_p, distances)==0) {
                hout << "Error in Get_penetrating_points while calling Is_below_cutoff for CNTs." << endl;
                return 0;
            } //else n_CNT++;
            //hout << "Check5.2 ";
        } else {
            //The GNP numbering starts at -1, thus to make the first index equal to 0, I need to
            //take the negative of the coordinate and subtract 1
            GNP2 = -1-global_coordinates[coord2][0];
            P2 = global_coordinates[coord2][1];
            //hout << "Check4b ";
            //The minimum distance allowied between a CNT point and a GNP point (cutoff) should be just cnt_rad+d_vdw
            //However, this is below the step length that usually is 2*cnt_rad. This could cause the next point to
            //be inside the GNP. Hence by making the cutoff=2*cnt_rad+d_vdw, it will be unlikely that the next point
            //is located inside the GNP
            cutoff_p = 2.0*cnt_rad + d_vdw;
            //Chek if the point is below the cutoff; if so, add it to the corresponding vectors
            if (Is_below_cutoff(gnps_points[GNP2][P2], point, cutoff_p, affected_points, cutoffs_p, distances)==0) {
                hout << "Error in Get_penetrating_points while calling Is_below_cutoff for CNTs." << endl;
                return 0;
            } //else n_GNP++;
            //hout << "Check5b ";
        }
    }
    //hout << "n_CNT="<<n_CNT<<" n_GNP="<<n_GNP<<endl;
    //hout << "Check6 " << endl;
    
    return 1;
}
//This function checks if a given point is below a given cutoff distance
//If the point is below the given cutoff distance, it is added to the vectors of cutoffs_p and distances
//which are used to calculate the new position of the overlapping points
int GenNetwork::Is_below_cutoff(const Point_3D &point_overlap, const Point_3D &point, const double &cutoff_p, vector<Point_3D> &affected_points, vector<double> &cutoffs_p, vector<double> &distances)const
{
    //Check if the second point is in the cube of size 2cutoff_p and centered in P1
    //This is easier and faster to check than calculating the distance from poin to point every time
    if ( (point_overlap.x<point.x+cutoff_p)&&(point_overlap.x>point.x-cutoff_p)&&(point_overlap.y<point.y+cutoff_p)&&(point_overlap.y>point.y-cutoff_p)&&(point_overlap.z<point.z+cutoff_p)&&(point_overlap.z>point.z-cutoff_p) ) {
        
        double distance = point.distance_to(point_overlap);
        //If it is inside the cube, then it is worth to take the time to calculate the distance from point ot point
        if (distance < cutoff_p) {
            affected_points.push_back(point_overlap);
            cutoffs_p.push_back(cutoff_p);
            distances.push_back(distance);
            /*/hout << "CNT=" << CNT1 << " Point=" << P1 << " r1=" << cnt_rad;
             hout << " Penetrating points="<< affected_points.size();
             hout << " CNT2=" << CNT2 << " P2=" << P2 << " r2=" << radii[CNT2] << " (" << cnts[CNT2][P2].x << ", " << cnts[CNT2][P2].y << ", " << cnts[CNT2][P2].z << ") ";
             hout << endl;//*/
        }
    }
    return 1;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//Generate a random value through a probability distribution function
int GenNetwork::Get_random_value_mt(const string &dist_type, mt19937 &engine, uniform_real_distribution<double> &dist, const double &min, const double &max, double &value)const
{
    //Check if limits are correctly defined
    if(min>max) { hout << "Error, the minimum value is larger than the maximum value (Get_random_value)!" << endl; return 0; }
    
    //Check if the interval has 0 length
    if (max == min) {
        //In this case, value is either of the limits
        //To be consistent with the formulation below, value is set equal to min
        value = min;
        return 1;
    }
    
    if(dist_type=="uniform")	//uniform distribution
    {
        value = (max-min)*dist(engine) + min;
    }
    else if(dist_type=="normal")	//normal distribution
    {
        double sum=0;
        for(int i=0; i<12; i++)
        {
            sum = sum + dist(engine);
        }
        value = (max-min)*sum/12.0 + min;
    }
    
    return 1;
}
//---------------------------------------------------------------------------
//Randomly generate a seed (intial point) of a CNT in the RVE
int GenNetwork::Get_seed_point_mt(const struct cuboid &cub, Point_3D &point, mt19937 &engine_x, mt19937 &engine_y, mt19937 &engine_z, uniform_real_distribution<double> &dist)const
{
    
    point.x = cub.poi_min.x + cub.len_x*dist(engine_x);
    
    point.y = cub.poi_min.y + cub.wid_y*dist(engine_y);
    
    point.z = cub.poi_min.z + cub.hei_z*dist(engine_z);
    
    
    point.flag = 0; //0 denotes this point is the initial point of a CNT
    
    return 1;
}
//---------------------------------------------------------------------------
//Randomly generate an initial direction, then generate the rotation matrix that results in that rotation
int GenNetwork::Get_initial_direction_mt(const string &dir_distrib_type, const double &ini_sita, const double &ini_pha, mt19937 &engine_inital_direction, uniform_real_distribution<double> &dist_initial, MathMatrix &rotation)const
{
    if(dir_distrib_type=="random")
    {
        //Choose three random numbers between -1 and 1,
        //they are the components of the vector that will define the initial direction
        double a = dist_initial(engine_inital_direction);
        double b = dist_initial(engine_inital_direction);
        double c = dist_initial(engine_inital_direction);
        
        //Check that a, b and c are not all zero
        if (abs(a) < Zero && abs(b) < Zero && abs(c) < 0) {
            //Then transform this to a simple case where a = b = c
            a = 1.0;
            b = 1.0;
            c = 1.0;
        }
        
        //Calculate the length of the vector v = (a,b,c)
        double v_length = sqrt(a*a + b*b + c*c);
        
        //This quantity is used three times:
        double quantity = sqrt(a*a + b*b);
        
        //Calculate the trigonometric functions of the angles sita and pha
        double cos_pha = a/quantity;
        double sin_pha = b/quantity;
        double cos_sita = c/v_length;
        double sin_sita = quantity/v_length;
        
        //Fill the elements of the rotation matrix
        rotation.element[0][0] = cos_pha*cos_sita;
        rotation.element[0][1] = -sin_pha;
        rotation.element[0][2] = cos_pha*sin_sita;
        
        rotation.element[1][0] = sin_pha*cos_sita;
        rotation.element[1][1] = cos_pha;
        rotation.element[1][2] = sin_pha*sin_sita;
        
        rotation.element[2][0] = -sin_sita;
        rotation.element[2][2] = cos_sita;
    }
    else if(dir_distrib_type=="specific")
    {
        //initialize variables with the initial direction
        double cnt_sita = ini_sita;
        double cnt_pha = ini_pha;
        
        //Use the probability of a random number to be even to use the opposite direction half the time
        if( engine_inital_direction()%2==0 )
        {
            cnt_sita = PI - ini_sita;		//"negative" (opposite) direction
            cnt_pha = PI + ini_pha;	//"negative" (opposite) direction
        }
        //Get the rotation matrix
        rotation = Get_transformation_matrix(cnt_sita, cnt_pha);
    }
    
    return 1;
}
//---------------------------------------------------------------------------
int GenNetwork::Get_normal_direction_mt(const double &omega, double &cnt_sita, double &cnt_pha, mt19937 &engine_sita, mt19937 &engine_pha, uniform_real_distribution<double> &dist)const
{
    
    //sita centers around 0 and obeys a normal distribution in (-omega, +omega)
    double sum=0;
    for(int i=0; i<12; i++)
    {
        sum = sum + dist(engine_sita);
    }
    cnt_sita = fabs(omega*(sum/6 - 1));
    
    //pha satisfies a uniform distribution in (0, 2PI)
    cnt_pha = 2.0*PI*dist(engine_pha);//*/
    
    return 1;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//This functions initializes the vectors n_subregions and sectioned_domain
//
//The n_subregions vector is defined to avoid calculating the number of sub-regions for every point when the functions
//Default_region and Add_to_subregion are called. Thus, saving computational time.
//n_subregions[0] is the number of subregions along x
//n_subregions[1] is the number of subregions along y
//n_subregions[2] is the number of subregions along z
//
//The vector sectioned_domain contains the sub-regions to look for overlapping
//It is initialized with the number of sub-regions in the sample
void GenNetwork::Initialize_subregions(const struct Geom_sample &geom_sample, vector<int> &nsubregions, vector<vector<long int> > &sectioned_domain)const
{
    //Initialize nsubregions
    
    //variable to store the number of subregions
    int s;
    //Number of subregions along x
    s = (int)(geom_sample.len_x/geom_sample.gs_minx);
    //Add the number of sub-regions to the vector
    nsubregions.push_back(s);
    //Number of subregions along y
    s = (int)(geom_sample.wid_y/geom_sample.gs_miny);
    //Add the number of sub-regions to the vector
    nsubregions.push_back(s);
    //Number of subregions along z
    s = (int)(geom_sample.hei_z/geom_sample.gs_minz);
    //Add the number of sub-regions to the vector
    nsubregions.push_back(s);
    
    //Initialize sectioned_domain
    vector<long int> empty;
    sectioned_domain.assign(nsubregions[0]*nsubregions[1]*nsubregions[2], empty);
}
//Check if the current CNT is penetrating another CNT, i.e. is the new point is overlapping other point
//1: a) No penetration
//   b) No need to check for penetration (point is in boundary layer or there are no other points in the same sub-region)
//   c) There was penetration but it was succesfully resolved
//0: There was penetration but could not be resolved
int GenNetwork::Check_penetration(const struct Geom_sample &geom_sample, const struct Nanotube_Geo &nanotube_geo, const vector<vector<Point_3D> > &cnts, const vector<vector<int> > &global_coordinates, const vector<vector<long int> > &sectioned_domain, const vector<double> &radii, const vector<Point_3D> &cnt_new, const vector<int> &n_subregions, const double &cnt_rad, const double &d_vdw, int &point_overlap_count, int &point_overlap_count_unique, Point_3D &point)const
{
    //Get the sub-region the point belongs to
    int subregion = Get_subregion(geom_sample, n_subregions, point);
    
    //If the sub-region is -1, then the point is in the boundary layer, so there is no need to check penetration
    if (subregion == -1) {
        return 1;
    }
    //This vector will store the coordintes of the points that the input "point" is penetrating
    vector<Point_3D> affected_points;
    //This vector stores the distance at which the two points should be
    vector<double> cutoffs_p;
    //This vector stores the distance at which the two points actually are
    vector<double> distances;
    //Need to keep the count of the number of attempts outside the for-loop
    int attempts;
    //I move the point up to max_attempts times. If there is still penetration then I delete it
    for (attempts = 0; attempts <= MAX_ATTEMPTS; attempts++) {
        
        //Check if there are any penetrations in the corresponding sub-region
        Get_penetrating_points(cnts, global_coordinates, sectioned_domain[subregion], radii, cnt_rad, d_vdw, point, affected_points, cutoffs_p, distances);
        
        //--------------------------------------------------------------------------------------------
        //Check if there are any penetrating points
        if (affected_points.size()) {
            //Update the counter of overlaps
            point_overlap_count++;
            
            //Update the counter of overlapping points only when an overlapping point was found the first time
            //i.e. attempts = 0
            if (!attempts) {
                point_overlap_count_unique++;
            }
            
            //If this is the last iteration and there are still affected points then the point could not be accommodated
            if (attempts == MAX_ATTEMPTS) {
                //hout << "Deleted CNT number " << cnts.size() << " of size " << cnt_new.size();
                //hout << " (reached maximum number of attempts for relocation)" << endl;//*/
                return 0;
            }
            
            //Find the new point
            //hout << "Point " << global_coordinates.size()-1+cnt_new.size() << " in CNT " << cnts.size() << " is overlapping." <<endl;
            //hout << "Moved a point from initial position (" << point.x << ", " << point.y << ", " << point.z << ")." << endl;
            Move_point(geom_sample, nanotube_geo, cnt_new, point, cutoffs_p, distances, affected_points);
            //hout << "Moved a point to final position (" << point.x << ", " << point.y << ", " << point.z << ")." << endl;
            
            //Check that the new point is within the permited orientation repect to the previous segment
            if (!Check_segment_orientation(point, cnt_new)) {
                //hout << "Deleted CNT number " << cnts.size() << " of size " << cnt_new.size();
                //hout << " (the point is not in a valid orientation)" << endl;//*/
                //When not in a valid position it cannot be moved again so a new CNT is needed
                return 0;
            }
            
            //Need to update point sub-region as it could be relocated to a new sub-region
            subregion = Get_subregion(geom_sample, n_subregions, point);
            //Check if after moving the point it is now in the boundary layer
            if (subregion == -1) {
                //If the point is now in the boundary layer, terminate the function
                //there is no need to continue checking
                return 1;
            }
            
            //Need to clear the vectors affected_points, contact_coordinates and temporal_contacts so they are used again with the new point
            affected_points.clear();
            cutoffs_p.clear();
            distances.clear();
            
        } else {
            //if the size of affected_points is zero, then terminate the function
            return 1;
        }
    }
    
    return 1;
}
//---------------------------------------------------------------------------
//This function returns the subregion a point belongs to
int GenNetwork::Get_subregion(const struct Geom_sample &geom_sample, const vector<int> &n_subregions, const Point_3D &point)const
{
    if (Judge_RVE_including_point(geom_sample, point)) {
        //These variables will give me the region cordinates of the region that a point belongs to
        int a, b, c;
        //Calculate the region-coordinates
        a = (int)((point.x-geom_sample.origin.x)/geom_sample.gs_minx);
        //Limit the value of a as it has to go from 0 to n_subregions[0]-1
        if (a == n_subregions[0]) a--;
        b = (int)((point.y-geom_sample.origin.y)/geom_sample.gs_miny);
        //Limit the value of b as it has to go from 0 to n_subregions[1]-1
        if (b == n_subregions[1]) b--;
        c = (int)((point.z-geom_sample.origin.z)/geom_sample.gs_minz);
        //Limit the value of c as it has to go from 0 to n_subregions[2]-1
        if (c == n_subregions[2]) c--;
        
        return (a + (b*n_subregions[0]) + (c*n_subregions[0]*n_subregions[1]));
    } else {
        //If the point is in the boundary layer, then there is no need to calculate its sub-region
        return -1;
    }
}
//This functions iterates over a sub-region and determines if there are any penetrating points
//If there are penetrating points, they are stored in the vector affected_points
void GenNetwork::Get_penetrating_points(const vector<vector<Point_3D> > &cnts, const vector<vector<int> > &global_coordinates, const vector<long int> &subregion_vec, const vector<double> &radii, const double &cnt_rad, const double &d_vdw, Point_3D &point, vector<Point_3D> &affected_points, vector<double> &cutoffs_p, vector<double> &distances)const
{
    //They are just intermediate variables and I only use them to make the code more readable
    long int coord2;
    int P2, CNT2;
    //cutoff_p is used for the cutoff between two points (of different CNTs), distance is the actual distance
    //between those two points
    double cutoff_p, distance;
    
    //hout << "Check0 " << subregion_vec.size() << ' ';
    //hout << "CNT1=" << point.flag <<" ("<< point.x<<", "<<point.y<< ", " <<point.z<<") r1=" << cnt_rad<<endl;
    for (long int i = 0; i < (long int)subregion_vec.size(); i++) {
        //hout << "Check1 " ;
        coord2 = subregion_vec[i];
        //hout << "coord2="<<coord2<<" global_coordinates.size()="<<global_coordinates.size()<<endl;
        CNT2 = global_coordinates[coord2][0];
        P2 = global_coordinates[coord2][1];
        //hout << "Check4 ";
        cutoff_p = cnt_rad + radii[CNT2] + d_vdw;
        //Check is the second point is in the cube of size 2cutoff_p and centered in P1
        //This is easier and faster to check than calculating the distance from poin to point every time
        if ( (cnts[CNT2][P2].x<point.x+cutoff_p)&&(cnts[CNT2][P2].x>point.x-cutoff_p)&&(cnts[CNT2][P2].y<point.y+cutoff_p)&&(cnts[CNT2][P2].y>point.y-cutoff_p)&&(cnts[CNT2][P2].z<point.z+cutoff_p)&&(cnts[CNT2][P2].z>point.z-cutoff_p) ) {
            distance = point.distance_to(cnts[CNT2][P2]);
            //If it is inside the cube, then it is worth to take the time to calculate the distance from point ot point
            if (distance < cutoff_p) {
                affected_points.push_back(cnts[CNT2][P2]);
                cutoffs_p.push_back(cutoff_p);
                distances.push_back(distance);
                //hout << "Penetrating point"<< affected_points.size()<< " CNT2=" << CNT2 << "("<<cnts[CNT2].size()<<" points) P2=" << P2 << " r2=" << radii[CNT2] << " (" << cnts[CNT2][P2].x << ", " << cnts[CNT2][P2].y << ", " << cnts[CNT2][P2].z << ") "<< endl;
            }
        }
        //hout << "Check5 ";
    }
    //hout << "Check6 " << endl;
}
//This function moves a point according to the number of points it is overlapping
void GenNetwork::Move_point(const struct Geom_sample &geom_sample, const struct Nanotube_Geo &nanotube_geo, const vector<Point_3D> &cnt_new, Point_3D &point, vector<double> &cutoffs, vector<double> &distances, vector<Point_3D> &affected_points)const
{
    //The number of overlapings will determine how the new point is moved
    //However, first I need to eliminate invalid-points, which are the points of perfect overlapping, i.e.
    //points in the exact same location
    int initial_overlappings = (int)affected_points.size();
    int overlappings = Check_points_in_same_position(cutoffs, distances, affected_points);
    if (overlappings == 1){
        //Handle the case with only one overlapping point
        //hout << "ovelappings == 1"<<endl;
        One_overlapping_point(cutoffs, distances, affected_points, point);
    } else if (overlappings == 2){
        //Handle the case with two overlapping points
        //hout << "ovelappings == 2"<<endl;
        Two_overlapping_points(cutoffs, affected_points, point);
    } else if (overlappings >= 3) {
        //Handle the case with three overlapping points
        //This actually finds the two closest point and calls the function
        //that handles the case with two overlapping points
        //hout << "ovelappings >= 3"<<endl;
        Three_or_more_overlapping_points(cutoffs, distances, affected_points, point);
    } else {//if (!ovelappings) {
        //If after cheking for points in the same position there are no ovelappings,
        //then all points are overlapping are in the same location
        //hout << "ovelappings == 0"<<endl;
        Overlapping_points_same_position(geom_sample, nanotube_geo, cnt_new, point);
        hout << "There were " << initial_overlappings - overlappings << " points overlapping in exactly the same location: ";
        hout << "P("<<point.x<<", "<<point.y<<", "<<point.z<<")."<<endl;
    }
}
//This point checks if two points are actually in the same position
//This used to happen because of a bug in the code. It seems now like a remote possibility
//so I'll keep it just in case
int GenNetwork::Check_points_in_same_position(vector<double> &cutoffs, vector<double> &distances, vector<Point_3D> &affected_points)const
{
    //hout << "Initial overlaps = " << distances.size();
    //check if any distance is less than the variable Zero
    for (int i = (int)distances.size()-1; i >= 0 ; i--) {
        if (distances[i] < Zero){
            //Delete points that are in the exact same location
            distances.erase(distances.begin()+i);
            cutoffs.erase(cutoffs.begin()+i);
            affected_points.erase(affected_points.begin()+i);
        }
    }
    //hout << ", valid overlaps = " << distances.size() <<endl;
    //This is the number of valid overlapping points
    return (int)distances.size();
}
//Move the point when all points are in the same location
//After solving the bug that caused multiple points to be in the same location,
//probably this function is not needed. I leave it here just in case
void GenNetwork::Overlapping_points_same_position(const struct Geom_sample &geom_sample, const struct Nanotube_Geo &nanotube_geo, const vector<Point_3D> &cnt_new, Point_3D &point)const
{
    //The new point will be moved depending on wheter it is the first point, second point or other point after the second
    if (!cnt_new.size()) {
        //Point is the first point
        //Move the point to a random location
        point.x = ((double)rand()/RAND_MAX)*geom_sample.ex_len + geom_sample.ex_origin.x;
        point.y = ((double)rand()/RAND_MAX)*geom_sample.ey_wid + geom_sample.ex_origin.y;
        point.z = ((double)rand()/RAND_MAX)*geom_sample.ez_hei + geom_sample.ex_origin.z;
    } else if (cnt_new.size() == 1) {
        //point is the second point
        //If there is only one point, just move the new point to a new direction
        
        //Generate a rotation matrix
        double phi = ((double)rand()/RAND_MAX)*2*PI;
        //For the second point, the limitation on the angles is not important since there are no other
        //segments to compare with
        double theta = ((double)rand()/RAND_MAX)*2*PI - PI;
        MathMatrix rotation(3,3);
        rotation = Get_transformation_matrix(theta, phi);
        
        //Calculate new point
        //The operation point = cnt_new.front() + Get_new_point(rotation, nanotube_geo.len_max)
        //has to be done in two steps because I get an error. The types of Point_3D are different
        //one is const and the other is not
        point = cnt_new.front();
        point = point + Get_new_point(rotation, nanotube_geo.len_max);
    } else {
        //point is the third point of higher
        //If there are 2 or more points, calculate the z_i unit vector and then move the new point
        //to a random direction
        
        //Generate a rotation matrix
        double phi = ((double)rand()/RAND_MAX)*2*PI;
        double theta = ((double)rand()/RAND_MAX)*PI - PI/2;
        MathMatrix rotation(3,3);
        rotation = Get_transformation_matrix(theta, phi);
        
        //Calculate the z_i unit vector
        //The operation Point_3D z_i = cnt_new.back() - cnt_new[cnt_new.size()-2];
        //has to be done in two steps because I get an error.
        Point_3D z_i = cnt_new.back();
        z_i = z_i - cnt_new[cnt_new.size()-2];
        z_i = z_i/(z_i.distance_to(z_i)); //unit vector
        
        //Calculate new point
        //temporary matrix to store a matrix vector multiplication
        MathMatrix vec(3,1);
        vec.element[0][0] = z_i.x;
        vec.element[1][0] = z_i.y;
        vec.element[2][0] = z_i.z;
        //Rotate unit vector, and multiply by the magnitude of the segment length
        vec = (rotation*vec)*nanotube_geo.step_length;
        //Add coordintes to last point in cnt_new to create new point
        point = cnt_new.back();
        point.x = point.x + vec.element[0][0];
        point.y = point.y + vec.element[1][0];
        point.z = point.z + vec.element[2][0];
        
    }
}
//This function finds the new location for an overlapping point when it overlaps only one point
void GenNetwork::One_overlapping_point(const vector<double> &cutoffs, const vector<double> &distances, const vector<Point_3D> &affected_points, Point_3D &point)const
{
    //When there is overlapping with one point only, then this is the simplest and easiest case
    //Just move the point in the direction form P_old to P_new a distance cutoff from P_old
    
    //Get the penetrating point. Its coordinates are in the first (and only) element of affected_points
    Point_3D P = affected_points[0];
    //Calculate direction unit vector. distances[0] already has the lenght of the vector form point to P
    Point_3D direction = (point - P)/distances[0];
    //So the new point is P_old + d*n. d is the cutoff and n the unit vector
    point = P + direction*(cutoffs[0]+Zero); //The Zero is to avoid machine precision errors. Without it, when comparing
    //the new point with the other points in the same region, the program was judging them to be below the cutoff
    //for the van der Waals distance. Even though they were in the limit. After adding this Zero that issue
    //was eliminated
}
//This function finds the new location for an overlapping point when it overlaps two points
void GenNetwork::Two_overlapping_points(const vector<double> &cutoffs, const vector<Point_3D> &affected_points, Point_3D &point)const
{
    //Point variables
    Point_3D P, Q, R, P1, P2;
    //distance variables
    double a, b, c, d;
    
    //Get the penetrating points.
    P1 = affected_points[0];
    //hout << "(" << affected_points[0].x << ", " << affected_points[0].y << ", " << affected_points[0].z << ")." << endl;
    P2 = affected_points[1];
    //hout << "(" << affected_points[1].x << ", " << affected_points[1].y << ", " << affected_points[1].z << ")." << endl;
    //Calculate P vector
    P = P2 - P1;
    //Calculate Q vector
    Q = point - P1;
    //Calculate normal vector PxQ
    R = (P.cross(Q)).cross(P);
    //Sides of the triangle
    a = cutoffs[0];
    b = cutoffs[1];
    c = P1.distance_to(P2);
    //Distance from P1 to M
    d = (b*b - a*a - c*c)/(-2*c);
    //Make P a unit vector
    P = P/sqrt(P.dot(P));
    //Make R a unit vector
    R = R/sqrt(R.dot(R));
    //Calculate new position
    point = P1 + P*(d + Zero) + R*(sqrt(a*a - d*d)+Zero);//The Zero is to avoid machine precision errors. Without it, when comparing
    //the new point with the other points in the same region, the program was judging them to be below the cutoff
    //for the van der Waals distance. Even though they were in the limit. After adding this Zero that issue
    //was eliminated
}
//This function finds the two closest points and calls the function that moves a point that overlaps two other points
//When a point overlaps three or more points, it becomes too difficult to find the new location
//Hence, this function that finds the two closest points
//The two closest point are chosen since those would be the more critical ones
void GenNetwork::Three_or_more_overlapping_points(const vector<double> &cutoffs, const vector<double> &distances, const vector<Point_3D> &affected_points, Point_3D &point)const
{
    //Use the distances vector to find the two closest points
    //i1 and i2 will be the indices of the closest affected_points, they will be initialized with the first two
    //i1 will have the index of the closest point, while i2 will have the index of the second closest point
    int i1, i2;
    //d1 and d2 will be the distances to the closest affected_points, they will be initialized with the first two
    //d1 will have the distance to the closest point, while d2 will have the distance to the second closest point
    double d1, d2;
    //Sort the first two distances
    if (distances[0] < distances[1]) {
        i1 = 0;
        d1 = distances[0];
        i2 = 1;
        d2 = distances[1];
    } else {
        i1 = 1;
        d1 = distances[1];
        i2 = 0;
        d2 = distances[0];
    }
    
    //Once the closest points have been initialized, then scan the rest of the affected points
    for (int i = 2; i < (int)distances.size(); i++) {
        //First check if the distances[i] is smaller than d2
        if (distances[i] < d2) {
            //In this case, distances[i] is one of the two closest point
            //Now I need to check against d1 in case distances[i] is the closest point now
            if (distances[i] < d1) {
                //distances[i] is the closest point
                //update the distances
                d2 = d1;
                d1 = distances[i];
                //update the indices
                i2 = i1;
                i1 = i;
            } else {
                //distances[i] is the second closest point
                //update the distances
                d2 = distances[i];
                i2 = i;
            }
        }
    }
    
    //Generate the necessary vectors for the case of two overlapping points
    vector<Point_3D> two_affected_points;
    two_affected_points.push_back(affected_points[i1]);
    two_affected_points.push_back(affected_points[i2]);
    vector<double> two_cutoffs;
    two_cutoffs.push_back(cutoffs[i1]);
    two_cutoffs.push_back(cutoffs[i2]);
    
    //Now, call the function that moves a point that overlaps two points
    Two_overlapping_points(two_cutoffs, two_affected_points, point);
}
//This function checks that "point" is within the bounds of the segment orientation.
//The criterion is just checking the point is not more than pi/2 respect with the previous
//segment. In the limiting case we have a straight triangle. So I calculate the hypotenuse.
//I also measure the distance between "point" and the second before that.
//If the distance  between points is less than the hypotenuse, then it has an
//incorrect orientation
int GenNetwork::Check_segment_orientation(const Point_3D &point, const vector<Point_3D> &cnt_new)const
{
    //If at least two points have already been generated, then check if the new point has a valid orientation
    if (cnt_new.size()>=2) {
        int last = (int)cnt_new.size()-1;
        
        //From the cosines law:
        //c^2 = a^2 + b^2 -2*a*b*cos(g)
        //where g is the angle between two consecutive segments, i.e., the angle between a and b
        
        //A valid angle is g > pi/2, so from the cosines law:
        //2*a*b*cos(g) = a^2 + b^2 - c^2 < 0 when g > pi/2
        //2*a*b*cos(g) = a^2 + b^2 - c^2 > 0 when g < pi/2
        
        //Thus an invalid angle happens when a^2 + b^2 - c^2 > 0
        //Check if this is the case
        if (Segment_angle_discriminant(point, cnt_new.back(), cnt_new[last-1]) > Zero) {
            //The point is not in a valid position
            return 0;
        } else {
            //The point is in a valid position
            return 1;
        }
    } else {
        //If point is the first or second point, its orientation does not matter
        return 1;
    }
}
//Given three points, this function calculates the sides of the triangle defined by these three points
//Then it returns the quantity a^2 + b^2 - c^2
//a = distance from first to second point
//b = distance from second to third point
//c = distance from first to third point
double GenNetwork::Segment_angle_discriminant(const Point_3D &first, const Point_3D &second, const Point_3D &third)const
{
    //calculate squared distances
    double a2 = first.squared_distance_to(second);
    double b2 = second.squared_distance_to(third);
    double c2 = first.squared_distance_to(third);
    
    return (a2 + b2 - c2);
}
//This function adds a point to a region so penetration can be checked
void GenNetwork::Add_to_overlapping_regions(const struct Geom_sample &geom_sample, double overlap_max_cutoff, Point_3D point, long int global_num, const vector<int> &n_subregions, vector<vector<long int> > &sectioned_domain)const
{
    //A point is added only if it is in the composite domain
    //If the point is in the boundary layer, overlapping is not important
    if (Judge_RVE_including_point(geom_sample, point)) {
        //Save coordinates of the point
        double x = point.x;
        double y = point.y;
        double z = point.z;
        
        //These variables will give me the region cordinates of the region that a point belongs to
        int a, b, c;
        //Calculate the region-coordinates
        a = (int)((x-geom_sample.origin.x)/geom_sample.gs_minx);
        //Limit the value of a as it has to go from 0 to n_subregions[0]-1
        if (a == n_subregions[0]) a--;
        b = (int)((y-geom_sample.origin.y)/geom_sample.gs_miny);
        //Limit the value of b as it has to go from 0 to n_subregions[1]-1
        if (b == n_subregions[1]) b--;
        c = (int)((z-geom_sample.origin.z)/geom_sample.gs_minz);
        //Limit the value of c as it has to go from 0 to n_subregions[2]-1
        if (c == n_subregions[2]) c--;
        
        //These variables are the coordinates of the lower corner of the RVE that defines its geometry
        double xmin = geom_sample.origin.x;
        double ymin = geom_sample.origin.y;
        double zmin = geom_sample.origin.z;
        
        //Coordinates of non-overlaping region the point belongs to
        double x1 = (double)a*geom_sample.gs_minx +  xmin;
        double x2 = x1 + geom_sample.gs_minx;
        double y1 = (double)b*geom_sample.gs_miny +  ymin;
        double y2 = y1 + geom_sample.gs_miny;
        double z1 = (double)c*geom_sample.gs_minz +  zmin;
        double z2 = z1 + geom_sample.gs_minz;
        
        //Initialize flags for overlaping regions
        int fx = 0;
        int fy = 0;
        int fz = 0;
        
        //Assign value of flag according to position of point
        //The first operand eliminates the periodicity on the boundary
        if ((a > 0) && (x >= x1) && (x <= x1+overlap_max_cutoff))
            fx = -1;
        else if ((a < n_subregions[0]-1) && (x >= x2-overlap_max_cutoff) && (x <= x2 ))
            fx = 1;
        if ((b > 0) && (y >= y1) && (y <= y1+overlap_max_cutoff))
            fy = -1;
        else if ((b < n_subregions[1]-1) && (y >= y2-overlap_max_cutoff) && (y <= y2 ))
            fy = 1;
        if ((c > 0) && (z >= z1) && (z <= z1+overlap_max_cutoff))
            fz = -1;
        else if ((c < n_subregions[2]-1) && (z >= z2-overlap_max_cutoff) && (z <= z2 ))
            fz = 1;
        
        //Create array for loop over overlaping regions
        int temp[2][3] = { {a+fx, b+fy, c+fz}, {a, b, c}};
        int t;
        
        //In this loop I check all regions a point can belong to when it is in an overlaping zone
        for (int ii = 0; ii < 2; ii++) {
            if (!fx) ii++; //if flag is zero, do this loop only once
            for (int jj = 0; jj < 2; jj++) {
                if (!fy) jj++; //if flag is zero, do this loop only once
                for (int kk = 0; kk < 2; kk++) {
                    if (!fz) kk++; //if flag is zero, do this loop only once
                    //hout <<"a="<<a<<" fx="<<fx<<" b="<<b<<" fy="<<fy<<" c="<<c<<" fz="<<fz;
                    t = Calculate_t(temp[ii][0],temp[jj][1],temp[kk][2],n_subregions[0],n_subregions[1]);
                    //hout<<" t="<<t<<" sectioned_domain["<<t<<"].size()="<<sectioned_domain[t].size();
                    sectioned_domain[t].push_back(global_num);
                    //hout<<'.'<<endl;
                }
            }
        }


    }
}
//Calculates the region to which a point corresponds
int GenNetwork::Calculate_t(int a, int b, int c, int sx, int sy)const
{
    return a + b*sx + c*sx*sy;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//Generate a number of ellipsoids
int GenNetwork::Get_ellip_clusters(const struct cuboid &cub, const struct Agglomerate_Geo &agg_geo)const
{
    double epsilon = 0.01;						//A ratio for extending ellipsoids
    double ellip_volume = 0.0;
    vector<struct elliparam> ellips;			//Define the temporary vector of ellipsoids for nanotube cluster zones
    double real_volume_fraction;				//Define the real volume fraction of ellips in the RVE
    
    const int N_times=1000;					//A maximum number for generation
    int times = 0;										//Count the number of generation
    do
    {
        //-------------------------------------------------------------
        //Ready to generate an ellipsoid
        struct elliparam ell_temp;
        //Generate the center point of an ellipsoid
        ell_temp.x=cub.poi_min.x + ((double)rand()/RAND_MAX)*cub.len_x;
        
        ell_temp.y=cub.poi_min.y + ((double)rand()/RAND_MAX)*cub.wid_y;
        
        ell_temp.z=cub.poi_min.z + ((double)rand()/RAND_MAX)*cub.hei_z;
        
        //Generate the lengths of half-axes of an ellipsoid
        ell_temp.a=agg_geo.amin + ((double)rand()/RAND_MAX)*(agg_geo.amax - agg_geo.amin);
        if(!(agg_geo.bmin==0&&agg_geo.cmin==0))
        {
            ell_temp.b = agg_geo.bmin + ((double)rand()/RAND_MAX)*(ell_temp.a - agg_geo.bmin);
            
            ell_temp.c = agg_geo.cmin + ((double)rand()/RAND_MAX)*(ell_temp.b - agg_geo.cmin);
        }
        else
        {
            ell_temp.b = ell_temp.a;
            ell_temp.c = ell_temp.a;
        }
        
        //Generate 9 angles: [(alpha1,beta1,gamma1),(alpha2,beta2,gamma2),(alpha3,beta3,gamma3)]
        //between three axes of ellipsoid (a,b,c) with three coordinate axes (ox,oy,oz)
        double alpha1 = ((double)rand()/RAND_MAX)*PI;
        double beta1 = 0;
        if(alpha1>PI/2.0)
        {
            beta1 = (alpha1-PI/2.0) + ((double)rand()/RAND_MAX)*2*(PI-alpha1);
        }
        else
        {
            beta1 = (PI/2.0-alpha1) + ((double)rand()/RAND_MAX)*2*alpha1;
        }
        
        ell_temp.alpha1	=	cos(alpha1);																//alpha1 is chosen from (0, PI)
        ell_temp.beta1	=	cos(beta1);																//beta1 is chosen from (pi/2-r1) to (pi/2+r1)
        ell_temp.gamma1 = pow(-1.0, fmod(rand(), 2.0)+1.0)*sqrt(1.0-pow(ell_temp.alpha1,2)-pow(ell_temp.beta1,2));	 //Calculate the value of gamma but randomly choose "positive" or "negative"
        double alpha2 = 0;																					//alpha2 is chosen from (pi/2-r1) to (pi/2+r1)
        if(alpha1>PI/2.0)
        {
            alpha2  = (alpha1-PI/2.0) + ((double)rand()/RAND_MAX)*2*(PI-alpha1);
        }
        else
        {
            alpha2  = (PI/2.0-alpha1) + ((double)rand()/RAND_MAX)*2*alpha1;
        }
        ell_temp.alpha2 = cos(alpha2);
        
        double A, B, C;
        A = 1+pow(ell_temp.beta1/ell_temp.gamma1,2);
        B = 2*(ell_temp.alpha1*ell_temp.alpha2*ell_temp.beta1)/pow(ell_temp.gamma1,2);
        C = pow(ell_temp.alpha1*ell_temp.alpha2/ell_temp.gamma1,2)+pow(ell_temp.alpha2,2)-1.0;
        
        ell_temp.beta2 = (-B+pow(-1.0, fmod(rand(),2.0)+1)*sqrt(pow(B,2)-4*A*C))/(2*A);
        ell_temp.gamma2 = -(ell_temp.beta1/ell_temp.gamma1)*ell_temp.beta2-(ell_temp.alpha1*ell_temp.alpha2/ell_temp.gamma1);
        
        double sign;
        sign = (ell_temp.alpha1*ell_temp.beta2)/fabs(ell_temp.alpha1*ell_temp.beta2);
        ell_temp.alpha3 = sign*sqrt(1-pow(ell_temp.alpha1,2)-pow(ell_temp.alpha2,2));
        ell_temp.beta3 = -(ell_temp.alpha1*ell_temp.beta1+ell_temp.alpha2*ell_temp.beta2)/ell_temp.alpha3;
        ell_temp.gamma3 = -(ell_temp.alpha1*ell_temp.gamma1+ell_temp.alpha2*ell_temp.gamma2)/ell_temp.alpha3;
        
        ell_temp.a = (1+epsilon)*ell_temp.a;          //Extend axes of ellipsoid a little bit for checking intersection or not
        ell_temp.b = (1+epsilon)*ell_temp.b;
        ell_temp.c = (1+epsilon)*ell_temp.c;
        
        //-------------------------------------------------------------
        //To check if an intersection happens between this ellipsoid with the surfaces of RVE or other generated ellipsoids
        double delt_h = ell_temp.c/50;						//Attention: if devided too much, it will spend too much computing time
        int k1 = (int)(sqrt(pow(ell_temp.a,2)+pow(ell_temp.b,2))/delt_h);
        int K = 4*(k1+1);
        double sita = 2*PI/K;
        
        //To check if an intersection happens between this ellipsoid with the surfaces of RVE
        for(int i=0; i<=K/2; i++)
        {
            int l1 = (int)(sqrt(pow(ell_temp.a*sin(i*sita),2)+pow(ell_temp.b*sin(i*sita),2))/delt_h);
            int L = 4*(l1+1);
            double phi = 2*PI/L;
            
            for(int j=1; j<=L; j++)
            {
                double x=ell_temp.a*sin(i*sita)*cos(j*phi);
                double y=ell_temp.b*sin(i*sita)*sin(j*phi);
                double z=ell_temp.c*cos(i*sita);
                
                double x1=ell_temp.x+x*ell_temp.alpha1+y*ell_temp.alpha2+z*ell_temp.alpha3;
                double y1=ell_temp.y+x*ell_temp.beta1+y*ell_temp.beta2+z*ell_temp.beta3;
                double z1=ell_temp.z+x*ell_temp.gamma1+y*ell_temp.gamma2+z*ell_temp.gamma3;
                
                if(x1-cub.poi_min.x<Zero||x1-cub.poi_min.x>cub.len_x-Zero||
                   y1-cub.poi_min.y<Zero||y1-cub.poi_min.y>=cub.wid_y-Zero||
                   z1-cub.poi_min.z<Zero||z1-cub.poi_min.z>=cub.hei_z-Zero)
                {
                    times=times+1;
                    goto gen_again;
                }
            }
        }
        //To check if an intersection happens between this ellipsoid with other generated ellipsoids
        for(int i=0; i<(int)ellips.size(); i++)
        {
            //Rough estimate
            double dist = sqrt(pow(ell_temp.x-ellips[i].x, 2) + pow(ell_temp.y-ellips[i].y, 2) + pow(ell_temp.z-ellips[i].z, 2));
            
            if(dist>ell_temp.a+ellips[i].a+Zero)
            {
                goto gene;
            }
            else if((dist<ell_temp.c+ellips[i].c+Zero))
            {
                times=times+1;
                goto gen_again;
            }
            else
            {
                //accurate estimate
                for(int j=1; j<=K/2; j++)
                {
                    int l1=(int)(sqrt(pow(ell_temp.a*sin(j*sita),2)+pow(ell_temp.b*sin(j*sita),2))/delt_h);
                    int L=4*(l1+1);
                    double phi=2*PI/L;
                    
                    for(int m=1;m<=L;m++)
                    {
                        double x=ell_temp.a*sin(j*sita)*cos(m*phi);
                        double y=ell_temp.b*sin(j*sita)*sin(m*phi);
                        double z=ell_temp.c*cos(j*sita);
                        
                        double x1=ell_temp.x+x*ell_temp.alpha1+y*ell_temp.alpha2+z*ell_temp.alpha3;
                        double y1=ell_temp.y+x*ell_temp.beta1+y*ell_temp.beta2+z*ell_temp.beta3;
                        double z1=ell_temp.z+x*ell_temp.gamma1+y*ell_temp.gamma2+z*ell_temp.gamma3;
                        
                        x=x1-ellips[i].x;
                        y=y1-ellips[i].y;
                        z=z1-ellips[i].z;
                        
                        x1=x*ellips[i].alpha1+y*ellips[i].beta1+z*ellips[i].gamma1;
                        y1=x*ellips[i].alpha2+y*ellips[i].beta2+z*ellips[i].gamma2;
                        z1=x*ellips[i].alpha3+y*ellips[i].beta3+z*ellips[i].gamma3;
                        
                        double f=pow(x1,2)/pow(ellips[i].a, 2)+pow(y1,2)/pow(ellips[i].b, 2)+pow(z1,2)/pow(ellips[i].c, 2)-1.0;
                        
                        if(f<0.0)
                        {
                            times=times+1;
                            goto gen_again;
                        }
                    }
                }
            }
        gene: ;
        }
        //---------------------------------------------------------------------
        //To clear the number of times and to insert an ellipsoid to the vector
        times=0;
        ellips.push_back(ell_temp);
        //---------------------------------------------------------------------
        //Calculate the sum of ellipsoid volume
        ellip_volume += 4*PI*ell_temp.a*ell_temp.b*ell_temp.c/(3*pow(1+epsilon, 3.0));
        real_volume_fraction = ellip_volume/cub.volume;
    gen_again:	;
    }while(times<=N_times&&real_volume_fraction<agg_geo.vol_fra_criterion);
    
    //---------------------------------------------------------------------
    //Shrink back to original ellipsoids
    for(int i=0; i<(int)ellips.size(); i++)
    {
        ellips[i].a=ellips[i].a/(1+epsilon);
        ellips[i].b=ellips[i].b/(1+epsilon);
        ellips[i].c=ellips[i].c/(1+epsilon);
    }
    //---------------------------------------------------------------------
    //Print the ellipsoid surfaces by grids
    if(agg_geo.print_key ==2)	Export_cluster_ellipsoids_mesh(cub, ellips);
    
    //---------------------------------------------------------------------
    //Export the data of ellipsoid surfaces
    if(agg_geo.print_key==1||agg_geo.print_key==2)	Export_cluster_ellipsoids_data(ellips, real_volume_fraction);
    
    //To print the number of ellipsoids and volume fraction
    hout << "    The number of clusters and the sum of their volume fraction:" << (int)ellips.size() << "  " << real_volume_fraction << endl;
    
    return 1;
}
//---------------------------------------------------------------------------
//Print the ellipsoid surfaces by grids
void GenNetwork::Export_cluster_ellipsoids_mesh(const struct cuboid &cub, const vector<struct elliparam> &ellips)const
{
    ofstream otec("Cluster_Ellipsoid_Mesh.dat");
    otec << "TITLE = Cluster_Ellipsoid_Mesh" << endl;
    otec << "VARIABLES = X, Y, Z" << endl;
    
    //---------------------------------------------------------------------------
    //Print the frame of RVE
    otec << "ZONE N=" << 8 << ", E=" << 1 << ", F=FEPOINT, ET=BRICK" << endl;
    double cell_x[2] = {cub.poi_min.x, cub.poi_min.x+cub.len_x};
    double cell_y[2] = {cub.poi_min.y, cub.poi_min.y+cub.wid_y};
    double cell_z[2] = {cub.poi_min.z, cub.poi_min.z+cub.hei_z};
    for(int i=0; i<2; i++)
        for(int j=0; j<2; j++)
            for(int k=0; k<2; k++)
            {
                otec << cell_x[i] << "  " << cell_y[j] << "  " << cell_z[k] << endl;
            }
    
    otec << "1 2 4 3 5 6 8 7" << endl;
    otec << endl;
    
    for(int i=0; i<(int)ellips.size(); i++)
    {
        const int num_sita = 20;
        const int num_phi = int(2*num_sita*ellips[i].a/ellips[i].c+0.5);		//Rounded to the nearest whole number
        otec << "ZONE I=" << num_phi+1 << ", J=" << num_sita+1 << ", K=1, F=POINT" << endl;
        double x, y, z;
        double x1, y1, z1;
        double sita = PI/num_sita;
        double phi=2*PI/num_phi;
        for(int j=0; j<=num_sita; j++)
        {
            for(int m=1; m<=num_phi; m++)
            {
                x=ellips[i].a*sin(j*sita)*cos(m*phi);
                y=ellips[i].b*sin(j*sita)*sin(m*phi);
                z=ellips[i].c*cos(j*sita);
                
                x1=ellips[i].x+x*ellips[i].alpha1+y*ellips[i].alpha2+z*ellips[i].alpha3;
                y1=ellips[i].y+x*ellips[i].beta1+y*ellips[i].beta2+z*ellips[i].beta3;
                z1=ellips[i].z+x*ellips[i].gamma1+y*ellips[i].gamma2+z*ellips[i].gamma3;
                
                otec << x1 << "  " << y1 << "  " << z1 << endl;
            }
            
            x=ellips[i].a*sin(j*sita)*cos(phi);
            y=ellips[i].b*sin(j*sita)*sin(phi);
            z=ellips[i].c*cos(j*sita);
            
            x1=ellips[i].x+x*ellips[i].alpha1+y*ellips[i].alpha2+z*ellips[i].alpha3;
            y1=ellips[i].y+x*ellips[i].beta1+y*ellips[i].beta2+z*ellips[i].beta3;
            z1=ellips[i].z+x*ellips[i].gamma1+y*ellips[i].gamma2+z*ellips[i].gamma3;
            
            otec << x1 << "  " << y1 << "  " << z1 << endl;
        }
        otec << endl;
    }
    otec.close();
}
//---------------------------------------------------------------------
//Export the data of ellipsoid surfaces
void GenNetwork::Export_cluster_ellipsoids_data(const vector<struct elliparam> &ellips, const double &ellip_ratio)const
{
    ofstream out("Cluster_Ellipsoids_Data.dat");
    out <<"%The number of clusters and the sum of their volume fraction" << endl;
    out << (int)ellips.size() << " " << ellip_ratio <<endl;
    out <<"%Print 15 parameters: center point (x,y,z), size of axis (a,b,c), angles (alpha1,beta1,gamma1; alpha2,beta2,gamma2; alpha3,beta3,gamma3)" << endl;
    for(int i=0; i<(int)ellips.size (); i++)
    {
        out	<< i << "  "
        << ellips[i].x << " " << ellips[i].y << " " << ellips[i].z << " "
        << ellips[i].a << " " << ellips[i].b << " " << ellips[i].c << " "
        << ellips[i].alpha1 << " " << ellips[i].beta1 << " " << ellips[i].gamma1 << " "
        << ellips[i].alpha2 << " " << ellips[i].beta2 << " " << ellips[i].gamma2 << " "
        << ellips[i].alpha3 << " " << ellips[i].beta3 << " " << ellips[i].gamma3 << " "
        << endl;
    }
    out.close();
}
//---------------------------------------------------------------------------
//Generate a number of sperical clusters in regular arrangement (Increase the number of clusters which cannot be achieved by random distribution)
int GenNetwork::Get_spherical_clusters_regular_arrangement(const struct cuboid &cub, struct Agglomerate_Geo &agg_geo)const
{
    int snum = 2;			//The number of spheres on each side of RVE
    double sd_x = 0.5*cub.len_x/snum;
    double sd_y = 0.5*cub.wid_y/snum;
    double sd_z = 0.5*cub.hei_z/snum;
    if(sd_x<=agg_geo.amin||sd_y<=agg_geo.amin||sd_z<=agg_geo.amin) { hout << "Error: the number of spheres on each side of RVE is too many, please check again." << endl; return 0; }
    
    double real_volume_fraction;				//Define the real volume fraction of ellips in the RVE
    double ellip_volume = 0.0;
    vector<struct elliparam> ellips;			//Define the temporary vector of ellipsoids for nanotube cluster zones
    for(int i=0; i<snum; i++)
        for(int j=0; j<snum; j++)
            for(int k=0; k<snum; k++)
            {
                //-------------------------------------------------------------
                //Generate a sphere
                struct elliparam ell_temp;
                ell_temp.x	=	(2*k+1)*sd_x;
                ell_temp.y	=	(2*j+1)*sd_y;
                ell_temp.z	=	(2*i+1)*sd_z;
                
                ell_temp.a=agg_geo.amin;
                ell_temp.b = ell_temp.a;
                ell_temp.c = ell_temp.a;
                
                //Generate 9 angles: [(alpha1,beta1,gamma1),(alpha2,beta2,gamma2),(alpha3,beta3,gamma3)]
                //between three axes of ellipsoid (a,b,c) with three coordinate axes (ox,oy,oz)
                double alpha1 = ((double)rand()/RAND_MAX)*PI;
                double beta1 = 0;
                if(alpha1>PI/2.0)
                {
                    beta1 = (alpha1-PI/2.0) + ((double)rand()/RAND_MAX)*2*(PI-alpha1);
                }
                else
                {
                    beta1 = (PI/2.0-alpha1) + ((double)rand()/RAND_MAX)*2*alpha1;
                }
                
                ell_temp.alpha1	=	cos(alpha1);																//alpha1 is chosen from (0, PI)
                ell_temp.beta1	=	cos(beta1);																//beta1 is chosen from (pi/2-r1) to (pi/2+r1)
                ell_temp.gamma1 = pow(-1.0, fmod(rand(), 2.0)+1.0)*sqrt(1.0-pow(ell_temp.alpha1,2)-pow(ell_temp.beta1,2));	  //Calculate the value of gamma but randomly choose "positive" or "negative"
                double alpha2 = 0;																					//alpha2 is chosen from (pi/2-r1) to (pi/2+r1)
                if(alpha1>PI/2.0)
                {
                    alpha2  = (alpha1-PI/2.0) + ((double)rand()/RAND_MAX)*2*(PI-alpha1);
                }
                else
                {
                    alpha2  = (PI/2.0-alpha1) + ((double)rand()/RAND_MAX)*2*alpha1;
                }
                ell_temp.alpha2 = cos(alpha2);
                
                double A, B, C;
                A = 1+pow(ell_temp.beta1/ell_temp.gamma1,2);
                B = 2*(ell_temp.alpha1*ell_temp.alpha2*ell_temp.beta1)/pow(ell_temp.gamma1,2);
                C = pow(ell_temp.alpha1*ell_temp.alpha2/ell_temp.gamma1,2)+pow(ell_temp.alpha2,2)-1.0;
                
                ell_temp.beta2 = (-B+pow(-1.0, fmod(rand(),2.0)+1)*sqrt(pow(B,2)-4*A*C))/(2*A);
                ell_temp.gamma2 = -(ell_temp.beta1/ell_temp.gamma1)*ell_temp.beta2-(ell_temp.alpha1*ell_temp.alpha2/ell_temp.gamma1);
                
                double sign;
                sign = (ell_temp.alpha1*ell_temp.beta2)/fabs(ell_temp.alpha1*ell_temp.beta2);
                ell_temp.alpha3 = sign*sqrt(1-pow(ell_temp.alpha1,2)-pow(ell_temp.alpha2,2));
                ell_temp.beta3 = -(ell_temp.alpha1*ell_temp.beta1+ell_temp.alpha2*ell_temp.beta2)/ell_temp.alpha3;
                ell_temp.gamma3 = -(ell_temp.alpha1*ell_temp.gamma1+ell_temp.alpha2*ell_temp.gamma2)/ell_temp.alpha3;
                
                //---------------------------------------------------------------------
                //To insert an ellipsoid to the vector
                ellips.push_back(ell_temp);
                
                //---------------------------------------------------------------------
                //Calculate the sum of ellipsoid volume
                ellip_volume += 4*PI*ell_temp.a*ell_temp.b*ell_temp.c/3;
                real_volume_fraction = ellip_volume/cub.volume;
            }
    
    //To check if the volume fraction is less than the criterion value
    if(real_volume_fraction<agg_geo.vol_fra_criterion)
    {
        hout << "The sum of volume fraction of spheres: " << real_volume_fraction;
        hout << " which is less than the criterion value: " << agg_geo.vol_fra_criterion << " , please check it again!" << endl;
        return 0;
    }
    
    //---------------------------------------------------------------------
    //Print the ellipsoid surfaces by grids
    if(agg_geo.print_key==2)	Export_cluster_ellipsoids_mesh(cub, ellips);
    
    //---------------------------------------------------------------------
    //Export the data of ellipsoid surfaces
    if(agg_geo.print_key==1||agg_geo.print_key==2)	Export_cluster_ellipsoids_data(ellips, real_volume_fraction);
    
    //To print the number of ellipsoids and volume fraction
    hout << "    The number of clusters and the sum of their volume fraction:" << (int)ellips.size() << "  " << real_volume_fraction << endl;
    
    return 1;
}
//---------------------------------------------------------------------------
//Transform angles into matrix
MathMatrix GenNetwork::Get_transformation_matrix(const double &sita, const double &pha)const
{
    //M = M_pha*M_sita
    //          |cos(pha) -sin(pha) 0|
    // M_pha  = |sin(pha)  cos(pha) 0|
    //          |   0         0     1|
    //
    //          | cos(sita)  0  sin(sita)|
    // M_sita = |     0      1      0    |
    //          |-sin(sita)  0  cos(sita)|
    //Calculate the matrix elements directly, instead of multiplying two matrices
    MathMatrix M(3,3);
    M.element[0][0] = cos(pha)*cos(sita);
    M.element[0][1] = -sin(pha);
    M.element[0][2] = cos(pha)*sin(sita);
    
    M.element[1][0] = sin(pha)*cos(sita);
    M.element[1][1] = cos(pha);
    M.element[1][2] = sin(pha)*sin(sita);
    
    M.element[2][0] = -sin(sita);
    M.element[2][2] = cos(sita);
    
    return M;
}
//---------------------------------------------------------------------------
//To calculate the coordinates of the new CNT point (transformation of coordinates)
Point_3D GenNetwork::Get_new_point(MathMatrix &Matrix, const double &Rad)const
{
    //Point = Matrix*v
    //v = [0; 0; Rad]
    //Calculate the new point directly
    Point_3D Point(Matrix.element[0][2]*Rad, Matrix.element[1][2]*Rad, Matrix.element[2][2]*Rad);
    
    return Point;
}
//---------------------------------------------------------------------------
//To judge if a point is included in a cuboid
int GenNetwork::Judge_RVE_including_point(const struct cuboid &cub, const Point_3D &point)const
{
    if(point.x<cub.poi_min.x||point.x>cub.poi_min.x+cub.len_x||
       point.y<cub.poi_min.y||point.y>cub.poi_min.y+cub.wid_y||
       point.z<cub.poi_min.z||point.z>cub.poi_min.z+cub.hei_z) return 0;
    
    return 1;
}
//---------------------------------------------------------------------------
//To judge if a point is included in a RVE
int GenNetwork::Judge_RVE_including_point(const struct Geom_sample &geom_sample, const Point_3D &point)const
{
    if(point.x<geom_sample.origin.x||point.x>geom_sample.origin.x+geom_sample.len_x||
       point.y<geom_sample.origin.y||point.y>geom_sample.origin.y+geom_sample.wid_y||
       point.z<geom_sample.origin.z||point.z>geom_sample.origin.z+geom_sample.hei_z) return 0;
    
    return 1;
}
//---------------------------------------------------------------------------
//Calculate all intersection points between the new segment and surfaces of RVE
//(using a parametric equatio:  the parameter 0<t<1, and sort all intersection points from the smaller t to the greater t)
int GenNetwork::Get_intersecting_point_RVE_surface(const struct cuboid &cub, const Point_3D &point0, const Point_3D &point1, vector<Point_3D> &ipoi_vec)const
{
    double t_temp[6];
    //The planes (surfaces of RVE) perpendicular to X axis
    t_temp[0] = (cub.poi_min.x - point0.x)/(point1.x - point0.x);
    t_temp[1] = (cub.poi_min.x + cub.len_x - point0.x)/(point1.x - point0.x);
    //The planes (surfaces of RVE) perpendicular to Y axis
    t_temp[2] = (cub.poi_min.y - point0.y)/(point1.y - point0.y);
    t_temp[3] = (cub.poi_min.y + cub.wid_y - point0.y)/(point1.y - point0.y);
    //The planes (surfaces of RVE) perpendicular to Z axis
    t_temp[4] = (cub.poi_min.z - point0.z)/(point1.z - point0.z);
    t_temp[5] = (cub.poi_min.z + cub.hei_z - point0.z)/(point1.z - point0.z);
    
    vector<double> t_ratio;
    for(int i=0; i<6; i++)
    {
        if(t_temp[i]>=0&&t_temp[i]<1)
        {
            //Binary insertion sort
            int left = 0;
            int right = (int)t_ratio.size()-1;
            while(right>=left)
            {
                int middle = (left + right)/2;
                if(fabs(t_ratio[middle] - t_temp[i])<Zero) goto T_Value_Same; //the case with same values
                else if(t_ratio[middle] > t_temp[i]) right = middle - 1;
                else left = middle + 1;
            }
            t_ratio.insert(t_ratio.begin()+left, t_temp[i]);	//insertion
        T_Value_Same: ;
        }
    }
    
    if((int)t_ratio.size()<1||(int)t_ratio.size()>3)
    {
        hout << "Error, the number of intersection points between the segement and the surfaces of RVE is " << (int)t_ratio.size() << ", less than one or more than three!" << endl;
        hout << "Cuboid P_min="<<cub.poi_min.x<<' '<<cub.poi_min.y<<' '<<cub.poi_min.z<<endl;
        hout << "Cuboid size="<<cub.len_x<<' '<<cub.wid_y<<' '<<cub.hei_z<<endl;
        hout << "P0= "<<point0.x<<' '<<point0.y<<' '<<point0.z<<' '<<endl;
        hout << "Judge_RVE_including_point="<<Judge_RVE_including_point(cub, point0)<<endl;
        hout << "P1= "<<point1.x<<' '<<point1.y<<' '<<point1.z<<' '<<endl;
        hout << "Judge_RVE_including_point="<<Judge_RVE_including_point(cub, point1)<<endl;
        return 0;
    }
    
    Point_3D point_temp;
    for(int i=0; i<(int)t_ratio.size(); i++)
    {
        point_temp.x = point0.x+(point1.x-point0.x)*t_ratio[i];
        point_temp.y = point0.y+(point1.y-point0.y)*t_ratio[i];
        point_temp.z = point0.z+(point1.z-point0.z)*t_ratio[i];
        point_temp.flag = 1;		//a temporary point
        
        //---------------------------------------------------------------------------
        //Error correction
        if(fabs(point_temp.x-cub.poi_min.x)<Zero) point_temp.x = cub.poi_min.x;
        else if(fabs(point_temp.x-cub.poi_min.x-cub.len_x)<Zero) point_temp.x = cub.poi_min.x + cub.len_x;
        
        if(fabs(point_temp.y-cub.poi_min.y)<Zero) point_temp.y = cub.poi_min.y;
        else if(fabs(point_temp.y-cub.poi_min.y-cub.wid_y)<Zero) point_temp.y = cub.poi_min.y + cub.wid_y;
        
        if(fabs(point_temp.z-cub.poi_min.z)<Zero) point_temp.z = cub.poi_min.z;
        else if(fabs(point_temp.z-cub.poi_min.z-cub.hei_z)<Zero) point_temp.z = cub.poi_min.z + cub.hei_z;
        
        //---------------------------------------------------------------------------
        //Insert a new point
        ipoi_vec.push_back(point_temp);
    }
    
    return 1;
}
//---------------------------------------------------------------------------
//To calculate the effective portion (length) which falls into the given region (RVE)
double GenNetwork::Effective_length_given_region(const struct cuboid &cub, const Point_3D last_point, const Point_3D new_point)const
{
    //Check if the last point is inside the given region
    int last_bool = Judge_RVE_including_point(cub, last_point);
    //Check if the new point is inside the given region
    int new_bool = Judge_RVE_including_point(cub, new_point);
    
    //Vector to store the intersecting point
    vector<Point_3D> ipoi_vec;
    
    //Decide the corresponding case and calculate volume fraction
    if (last_bool&&new_bool)
        return last_point.distance_to(new_point); //both points are inside so add the total length
    else if (last_bool&&(!new_bool))  //if the last point is inside and the new point is outside
    {
        if(Get_intersecting_point_RVE_surface(cub, last_point, new_point, ipoi_vec)==0){
            hout << "Error in Effective_length_given_region, case last_bool&&(!new_bool) "<<endl;
            return 0;
        }
        return last_point.distance_to(ipoi_vec[0]);
    }
    else if ((!last_bool)&&new_bool)  //if the last point is outside and the new point is inside
    {
        if(Get_intersecting_point_RVE_surface(cub, new_point, last_point, ipoi_vec)==0) {
            hout << "Error in Effective_length_given_region, case (!last_bool)&&new_bool"<<endl;
            return 0;
        }
        return new_point.distance_to(ipoi_vec[0]);
    }
    else
        return 0.0; //if both points are outside
}
//---------------------------------------------------------------------------
//Checking the angle between any two segments in each nanotube (if less than PI/2, send error message)
int GenNetwork::CNTs_quality_testing(const vector<vector<Point_3D> > &cnts_points)const
{
    //---------------------------------------------------------------------------
    //Checking the angle between two segments
    for(int i=0; i<(int)cnts_points.size(); i++)
    {
        for (int j=1; j<(int)cnts_points[i].size()-1; j++)
        {
            //From the cosines law:
            //c^2 = a^2 + b^2 -2*a*b*cos(g)
            //where g is the angle between two consecutive segments, i.e., the angle between a and b
            
            //A valid angle is g > pi/2, so from the cosines law:
            //2*a*b*cos(g) = a^2 + b^2 - c^2 < 0 when g > pi/2
            //2*a*b*cos(g) = a^2 + b^2 - c^2 > 0 when g < pi/2
            
            //Thus an invalid angle happens when a^2 + b^2 - c^2 > 0
            //Check if this is the case
            if( Segment_angle_discriminant(cnts_points[i][j-1], cnts_points[i][j], cnts_points[i][j+1]) > Zero )
            {
                hout << "Error: there exists at least one angle which is smaller than PI/2!" << endl;
                hout << setwp(1,20)<<"a^2 + b^2 - c^2="<<Segment_angle_discriminant(cnts_points[i][j-1], cnts_points[i][j], cnts_points[i][j+1])<<endl;
                hout << "CNT# = "<<i<<endl;
                hout << "P1 ("<<j-1<<") = "<<cnts_points[i][j-1].x<<' '<<cnts_points[i][j-1].y<<' '<<cnts_points[i][j-1].z<<endl;
                hout << "P2 ("<<j<<") = "<<cnts_points[i][j].x<<' '<<cnts_points[i][j].y<<' '<<cnts_points[i][j].z<<endl;
                hout << "P3 ("<<j+1<<") = "<<cnts_points[i][j+1].x<<' '<<cnts_points[i][j+1].y<<' '<<cnts_points[i][j+1].z<<endl;
                hout << "P1P2 = a =" << cnts_points[i][j-1].distance_to(cnts_points[i][j]) << endl;
                hout << "P2P3 = b =" << cnts_points[i][j].distance_to(cnts_points[i][j+1]) << endl;
                hout << "P1P3 = c =" << cnts_points[i][j-1].distance_to(cnts_points[i][j+1]) << endl;
                hout << endl;                
                
                return 0;
            }
        }
    }
    
    return 1;
}
//---------------------------------------------------------------------------
//Generate the nodes and tetrahedron elements of nanotubes (No const following this function because a sum operation on two Point_3D points inside)
int GenNetwork::Generate_cnts_nodes_elements(vector<vector<Node> > &nodes, vector<vector<Element> > &eles, const vector<vector<Point_3D> > &cnts_points, const vector<double> &cnts_radius)
{
    //Looping the generated nanotubes
    for(int i=0; i<(int)cnts_points.size(); i++)
    {
        vector<Node> nod_temp;
        vector<Element> ele_temp;
        
        const int cps = (int)cnts_points[i].size();
        for(int j=0; j<cps; j++)
        {
            //Calculate the normal vector of the plane
            Point_3D plane_normal;
            if(j==0)
            {
                plane_normal.x = cnts_points[i][j].x - cnts_points[i][j+1].x;
                plane_normal.y = cnts_points[i][j].y - cnts_points[i][j+1].y;
                plane_normal.z = cnts_points[i][j].z - cnts_points[i][j+1].z;
            }
            else if(j==cps-1)
            {
                plane_normal.x = cnts_points[i][j-1].x - cnts_points[i][j].x;
                plane_normal.y = cnts_points[i][j-1].y - cnts_points[i][j].y;
                plane_normal.z = cnts_points[i][j-1].z - cnts_points[i][j].z;
            }
            else
            {
                const Point_3D vect[3] = { cnts_points[i][j-1], cnts_points[i][j], cnts_points[i][j+1] };
                const double A = pow(vect[0].x-vect[1].x,2)+pow(vect[0].y-vect[1].y,2)+pow(vect[0].z-vect[1].z,2);
                const double B = pow(vect[2].x-vect[1].x,2)+pow(vect[2].y-vect[1].y,2)+pow(vect[2].z-vect[1].z,2);
                const double tt = sqrt(A/B);
                
                //Coordinate transformation to find the intersection points
                double x, y, z;
                x=vect[1].x+tt*(vect[2].x-vect[1].x);
                y=vect[1].y+tt*(vect[2].y-vect[1].y);
                z=vect[1].z+tt*(vect[2].z-vect[1].z);
                
                plane_normal.x = vect[0].x - x;
                plane_normal.y = vect[0].y - y;
                plane_normal.z = vect[0].z - z;
            }
            //The center point of the circle on the plane
            Point_3D plane_center = cnts_points[i][j];
            
            //Define the number of sections along the circumference
            const int num_sec = 36;
            if(j==0)
            {
                double normal_sita, normal_pha;  //Direction angles
                //Calculate the angles of the normal verctor of the plane in the spherical coordinate
                if(Get_angles_vector_in_spherial_coordinates(plane_normal, normal_sita, normal_pha)==0) return 0;
                
                //Calculate a group of equidistant points along the circumference which is on the plane defined by the center point of the circle and the normal vector
                if(Get_points_circle_in_plane(plane_center, normal_sita, normal_pha, cnts_radius[i], num_sec, nod_temp)==0) return 0;
            }
            else
            {
                //Calculate a group of projected points (which are on the plane with the center point of the circle and the normal vector)
                //which are projected from a group of points on the previous circumference and projected along the direction of line_vec
                Point_3D line_vec;
                line_vec.x = cnts_points[i][j-1].x - cnts_points[i][j].x;
                line_vec.y = cnts_points[i][j-1].y - cnts_points[i][j].y;
                line_vec.z = cnts_points[i][j-1].z - cnts_points[i][j].z;
                if(Get_projected_points_in_plane(plane_center, plane_normal, line_vec, num_sec, nod_temp)==0) return 0;
            }
            
            //Generate a vector of elements
            if(j!=0)
            {
                int nodes_num[6];
                nodes_num[0] = (j-1)*(num_sec+1);   //The number of the center
                nodes_num[3] = j*(num_sec+1);
                for(int k=1; k<=num_sec; k++)
                {
                    nodes_num[1] = (j-1)*(num_sec+1) + k;
                    nodes_num[2] = (j-1)*(num_sec+1) + 1 + k%num_sec;
                    nodes_num[4] = j*(num_sec+1) + k;
                    nodes_num[5] = j*(num_sec+1) + 1 + k%num_sec;
                    
                    Element eles_num[3];
                    //----------------------------------------------------------------
                    //Insert the numbers of nodes to the elements
                    eles_num[0].nodes_id.push_back(nodes_num[0]);
                    eles_num[0].nodes_id.push_back(nodes_num[1]);
                    eles_num[0].nodes_id.push_back(nodes_num[2]);
                    eles_num[0].nodes_id.push_back(nodes_num[3]);
                    
                    eles_num[1].nodes_id.push_back(nodes_num[1]);
                    eles_num[1].nodes_id.push_back(nodes_num[2]);
                    eles_num[1].nodes_id.push_back(nodes_num[3]);
                    eles_num[1].nodes_id.push_back(nodes_num[5]);
                    
                    eles_num[2].nodes_id.push_back(nodes_num[1]);
                    eles_num[2].nodes_id.push_back(nodes_num[3]);
                    eles_num[2].nodes_id.push_back(nodes_num[4]);
                    eles_num[2].nodes_id.push_back(nodes_num[5]);
                    //----------------------------------------------------------------
                    //Insert the number of elements to element vector
                    ele_temp.push_back(eles_num[0]);
                    ele_temp.push_back(eles_num[1]);
                    ele_temp.push_back(eles_num[2]);
                }
            }
        }
        
        nodes.push_back(nod_temp);
        eles.push_back(ele_temp);
    }
    
    return 1;
}
//---------------------------------------------------------------------------
//Generate the nodes and tetrahedron elements of nanotubes (No const following this function because a sum operation on two Point_3D points inside). This function uses a 1D point vector and a 2D structure vector that references the point vector
int GenNetwork::Generate_cnts_nodes_elements(vector<vector<Node> > &nodes, vector<vector<Element> > &eles, const vector<Point_3D> &cnts_points, const vector<double> &cnts_radius, const vector<vector<long int> > &structure)
{
    //Looping the generated nanotubes
    for(int i=0; i<(int)structure.size(); i++)
    {
        vector<Node> nod_temp;
        vector<Element> ele_temp;
        
        const int cps = (int)structure[i].size();
        for(int j=0; j<cps; j++)
        {
            //Calculate the normal vector of the plane
            Point_3D plane_normal;
            if(j==0)
            {
                long int P1 = structure[i][j];
                long int P2 = structure[i][j+1];
                plane_normal.x = cnts_points[P1].x - cnts_points[P2].x;
                plane_normal.y = cnts_points[P1].y - cnts_points[P2].y;
                plane_normal.z = cnts_points[P1].z - cnts_points[P2].z;
            }
            else if(j==cps-1)
            {
                long int P1 = structure[i][j-1];
                long int P2 = structure[i][j];
                plane_normal.x = cnts_points[P1].x - cnts_points[P2].x;
                plane_normal.y = cnts_points[P1].y - cnts_points[P2].y;
                plane_normal.z = cnts_points[P1].z - cnts_points[P2].z;
            }
            else
            {
                long int P1 = structure[i][j-1];
                long int P2 = structure[i][j];
                long int P3 = structure[i][j+1];
                const Point_3D vect[3] = { cnts_points[P1], cnts_points[P2], cnts_points[P3] };
                const double A = pow(vect[0].x-vect[1].x,2)+pow(vect[0].y-vect[1].y,2)+pow(vect[0].z-vect[1].z,2);
                const double B = pow(vect[2].x-vect[1].x,2)+pow(vect[2].y-vect[1].y,2)+pow(vect[2].z-vect[1].z,2);
                const double tt = sqrt(A/B);
                
                //Coordinate transformation to find the intersection points
                double x, y, z;
                x=vect[1].x+tt*(vect[2].x-vect[1].x);
                y=vect[1].y+tt*(vect[2].y-vect[1].y);
                z=vect[1].z+tt*(vect[2].z-vect[1].z);
                
                plane_normal.x = vect[0].x - x;
                plane_normal.y = vect[0].y - y;
                plane_normal.z = vect[0].z - z;
            }
            //The center point of the circle on the plane
            Point_3D plane_center = cnts_points[structure[i][j]];
            
            //Define the number of sections along the circumference
            const int num_sec = 36;
            if(j==0)
            {
                double normal_sita, normal_pha;  //Direction angles
                //Calculate the angles of the normal verctor of the plane in the spherical coordinate
                if(Get_angles_vector_in_spherial_coordinates(plane_normal, normal_sita, normal_pha)==0) return 0;
                
                //Calculate a group of equidistant points along the circumference which is on the plane defined by the center point of the circle and the normal vector
                if(Get_points_circle_in_plane(plane_center, normal_sita, normal_pha, cnts_radius[i], num_sec, nod_temp)==0) return 0;
            }
            else
            {
                //Calculate a group of projected points (which are on the plane with the center point of the circle and the normal vector)
                //which are projected from a group of points on the previous circumference and projected along the direction of line_vec
                Point_3D line_vec;
                long int P1 = structure[i][j-1];
                long int P2 = structure[i][j];
                line_vec.x = cnts_points[P1].x - cnts_points[P2].x;
                line_vec.y = cnts_points[P1].y - cnts_points[P2].y;
                line_vec.z = cnts_points[P1].z - cnts_points[P2].z;
                if(Get_projected_points_in_plane(plane_center, plane_normal, line_vec, num_sec, nod_temp)==0) return 0;
            }
            
            //Generate a vector of elements
            if(j!=0)
            {
                int nodes_num[6];
                nodes_num[0] = (j-1)*(num_sec+1);   //The number of the center
                nodes_num[3] = j*(num_sec+1);
                for(int k=1; k<=num_sec; k++)
                {
                    nodes_num[1] = (j-1)*(num_sec+1) + k;
                    nodes_num[2] = (j-1)*(num_sec+1) + 1 + k%num_sec;
                    nodes_num[4] = j*(num_sec+1) + k;
                    nodes_num[5] = j*(num_sec+1) + 1 + k%num_sec;
                    
                    Element eles_num[3];
                    //----------------------------------------------------------------
                    //Insert the numbers of nodes to the elements
                    eles_num[0].nodes_id.push_back(nodes_num[0]);
                    eles_num[0].nodes_id.push_back(nodes_num[1]);
                    eles_num[0].nodes_id.push_back(nodes_num[2]);
                    eles_num[0].nodes_id.push_back(nodes_num[3]);
                    
                    eles_num[1].nodes_id.push_back(nodes_num[1]);
                    eles_num[1].nodes_id.push_back(nodes_num[2]);
                    eles_num[1].nodes_id.push_back(nodes_num[3]);
                    eles_num[1].nodes_id.push_back(nodes_num[5]);
                    
                    eles_num[2].nodes_id.push_back(nodes_num[1]);
                    eles_num[2].nodes_id.push_back(nodes_num[3]);
                    eles_num[2].nodes_id.push_back(nodes_num[4]);
                    eles_num[2].nodes_id.push_back(nodes_num[5]);
                    //----------------------------------------------------------------
                    //Insert the number of elements to element vector
                    ele_temp.push_back(eles_num[0]);
                    ele_temp.push_back(eles_num[1]);
                    ele_temp.push_back(eles_num[2]);
                }
            }
        }
        
        nodes.push_back(nod_temp);
        eles.push_back(ele_temp);
    }
    
    return 1;
}
//---------------------------------------------------------------------------
//Calculate the angles of a verctor in the spherical coordinate
int GenNetwork::Get_angles_vector_in_spherial_coordinates(const Point_3D &normal, double &sita, double &pha)const
{
    if(normal.x==0&&normal.y==0&&normal.z==0) { hout << "Error, three elements of the vector are all zero!" << endl; return 0; }
    sita =  acos(normal.z/sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z));
    if(normal.x==0&&normal.y==0) pha = 0;
    else if(normal.y>=0) pha = acos(normal.x/sqrt(normal.x*normal.x+normal.y*normal.y));
    else if(normal.y<0) pha = 2*PI - acos(normal.x/sqrt(normal.x*normal.x+normal.y*normal.y));
    
    return 1;
}
//---------------------------------------------------------------------------
//Calculate a group of equidistant points along the circumference which is on the plane defined by the center point of the circle and the normal vector
int GenNetwork::Get_points_circle_in_plane(const Point_3D &center, const double &trans_sita, const double &trans_pha, const double &radius, const int &num_sec, vector<Node> &nod_temp)const
{
    //Insert the center point firstly
    Node new_node(center.x, center.y, center.z);
    nod_temp.push_back(new_node);
    
    //Define the transformation matrix
    MathMatrix trans_mat(3,3);
    trans_mat = Get_transformation_matrix(trans_sita, trans_pha);
    
    //1D vector defined by a matrix
    MathMatrix Rvec(3,1);
    Rvec.element[0][0] = 0;
    Rvec.element[1][0] = 0;
    Rvec.element[2][0] = radius;
    
    //1D vector defined by a matrix
    MathMatrix Res(3,1);
    
    double sita, pha;
    sita = 0.5*PI;	//Defined on the XOY plane
    for(int i=0; i<num_sec; i++)
    {
        pha = i*2*PI/num_sec;
        MathMatrix matrix_temp = trans_mat*Get_transformation_matrix(sita, pha);
        Res = matrix_temp*Rvec;
        
        new_node.x = center.x + Res.element[0][0];
        new_node.y = center.y + Res.element[1][0];
        new_node.z = center.z + Res.element[2][0]; 
        
        //Insert the points on the circumference
        nod_temp.push_back(new_node);
    }
    
    return 1;
}
//---------------------------------------------------------------------------
//Calculate a group of projected points (which are on the plane with the center point of the circle and the normal vector) 
//which are projected from a group of points on the previous circumference and projected along the direction of line_vec
int GenNetwork::Get_projected_points_in_plane(const Point_3D &center, const Point_3D &normal, const Point_3D &line, const int &num_sec, vector<Node> &nod_temp)const
{
    //Record the total number of nodes after the previous generation
    const int nod_size = (int)nod_temp.size();  
    
    //Insert the center point
    Node new_node(center.x, center.y, center.z);
    nod_temp.push_back(new_node);
    
    const double vectors_dot_product = normal.x*line.x+normal.y*line.y+normal.z*line.z;
    
    if(vectors_dot_product==0.0) 
    {
        //Corresponding to three points: number 0, 1 and 2, the peak of this angle is at the point number 1. 
        hout << "Error: these two normal vectors are perpendicular to each other!" << endl;
        return 0; 
    }
    
    for(int i=num_sec; i>0; i--)
    {
        Point_3D point(center.x-nod_temp[nod_size-i].x, center.y-nod_temp[nod_size-i].y, center.z-nod_temp[nod_size-i].z);
        new_node.x = nod_temp[nod_size-i].x + (normal.x*point.x+normal.y*point.y+normal.z*point.z)*line.x/ vectors_dot_product;
        new_node.y = nod_temp[nod_size-i].y + (normal.x*point.x+normal.y*point.y+normal.z*point.z)*line.y/ vectors_dot_product;
        new_node.z = nod_temp[nod_size-i].z + (normal.x*point.x+normal.y*point.y+normal.z*point.z)*line.z/ vectors_dot_product;
        
        //Insert the points on the circumference
        nod_temp.push_back(new_node);
    }
    
    return 1;
}
//---------------------------------------------------------------------------
//Transform the 2D cnts_points into 1D cpoints and 2D cstructures
int GenNetwork::Transform_points(const string &type, vector<vector<Point_3D> > &cnts_points, vector<Point_3D> &cpoints, vector<vector<long int> > &cstructures)const
{
    long int count = 0;
    for(int i=0; i<(int)cnts_points.size(); i++)
    {
        vector<long int> struct_temp;
        for(int j=0; j<(int)cnts_points[i].size(); j++)
        {
            cpoints.push_back(cnts_points[i][j]);
            cpoints.back().flag = i;
            struct_temp.push_back(count);
            count++;
        }
        cstructures.push_back(struct_temp);
        //Free some memory
        cnts_points[i].clear();
    }
    
    if (cnts_points.size()) {
        hout << "There are "<<cnts_points.size()<<" "<<type<<" with "<<cpoints.size() << " points."<<endl;
    }
    
    return 1;
}
//===========================================================================

#define EIGEN_DONT_VECTORIZE
#define EIGEN_DISABLE_UNALIGNED_ARRAY_ASSERT
#include <Eigen/Eigen>
#include <Eigen/LU>
#include <iostream>
#include <fstream>
#include <cmath>
#include <limits>
#include <vector>
#include <map>

using namespace std;

double uniformRand()
{
	return double(rand())/RAND_MAX;
}

double gaussianRand(double mean, double sigm)
{
	// Generation using the Polar (Box-Mueller) method.
	// Code inspired by GSL, which is a really great math lib.
	// http://sources.redhat.com/gsl/
	// C++ wrapper available.
	// http://gslwrap.sourceforge.net/
	double r, x, y;

	// Generate random number in unity circle.
	do
	{
		x = uniformRand()*2 - 1;
		y = uniformRand()*2 - 1;
		r = x*x + y*y;
	}
	while (r > 1.0 || r == 0);

	// Box-Muller transform.
	return sigm * y * sqrt (-2.0 * log(r) / r) + mean;
}

struct TrainingEntry
{
	double timeStamp;
	Eigen::Vector3d odom_tr;
	Eigen::eigen2_Quaterniond odom_rot;
	Eigen::Vector3d icp_tr;
	Eigen::eigen2_Quaterniond icp_rot;

	TrainingEntry(){}
	TrainingEntry(std::istream& is)
	{
		is >> timeStamp;
		double t_x = 0, t_y = 0, t_z = 0, q_x = 0, q_y = 0, q_z = 0, q_w = 1;
		is >> t_x;
		is >> t_y;
		is >> t_z;
		icp_tr = Eigen::Vector3d(t_x, t_y, t_z);
		is >> q_x;
		is >> q_y;
		is >> q_z;
		is >> q_w;
		icp_rot = Eigen::eigen2_Quaterniond(q_w, q_x, q_y, q_z).normalized();
		is >> t_x;
		is >> t_y;
		is >> t_z;
		odom_tr = Eigen::Vector3d(t_x, t_y, t_z);
		is >> q_x;
		is >> q_y;
		is >> q_z;
		is >> q_w;
		odom_rot = Eigen::eigen2_Quaterniond(q_w, q_x, q_y, q_z).normalized();
		//odom_tr = odom_rot*odom_tr;
		//cerr << icp_rot.x() << " " << icp_rot.y() << " " << icp_rot.z() << " " << icp_rot.w() <<endl;
		//cerr << icp_tr.x() << " " << icp_tr.y() << " " << icp_tr.z() << endl;
		// FIXME: bug in Eigen ?
		//icp_tr = icp_rot*icp_tr;
		//cerr << icp_tr.x() << " " << icp_tr.y() << " " << icp_tr.z() << endl;
	}

	void dump(ostream& stream) const
	{
		stream << 
			timeStamp << " : " << 
			icp_tr.x() << " " << icp_tr.y() << " " << icp_tr.z() << " " <<
			icp_rot.x() << " " << icp_rot.y() << " " << icp_rot.z() << " " << icp_rot.w() << " " <<
			odom_tr.x() << " " << odom_tr.y() << " " << odom_tr.z() << " " <<
			odom_rot.x() << " " << odom_rot.y() << " " << odom_rot.z() << " " << odom_rot.w();
	}
};

struct TrainingSet: public std::vector<TrainingEntry>
{
	void dump()
	{
		for (TrainingSet::const_iterator it(begin()); it != end(); ++it)
		{
			const TrainingEntry& entry(*it);
			entry.dump(cout);
			cout << "\n";
		}
	}
};

TrainingSet trainingSet;

struct Params
{
	Eigen::Vector3d tr;
	Eigen::eigen2_Quaterniond rot;
	
	//Params():tr(0,0,0),rot(1,0,0,0) {}
	Params():
		tr(
			gaussianRand(0, 0.5),
			gaussianRand(0, 0.5),
			gaussianRand(0, 0.5)
		),
		rot(
			Eigen::eigen2_Quaterniond(1,0,0,0) *
			Eigen::eigen2_Quaterniond(Eigen::eigen2_AngleAxisd(uniformRand() * M_PI*2, Eigen::Vector3d::UnitX())) *
			Eigen::eigen2_Quaterniond(Eigen::eigen2_AngleAxisd(uniformRand() * M_PI*2, Eigen::Vector3d::UnitY())) *
			Eigen::eigen2_Quaterniond(Eigen::eigen2_AngleAxisd(uniformRand() * M_PI*2, Eigen::Vector3d::UnitZ()))
		)
		{}
	
	// add random noise
	const Params& mutate(double amount = 1.0)
	{
		double count = fabs(gaussianRand(0, 1));
		for (int i = 0; i < int(count + 1); i++)
		{
			int toMutate = rand() % 6;
			switch (toMutate)
			{
				case 0: tr.x() += gaussianRand(0, 0.1) * amount; break;
				case 1: tr.y() += gaussianRand(0, 0.1) * amount; break;
				case 2: tr.z() += gaussianRand(0, 0.1) * amount; break;
				case 3: rot *= Eigen::eigen2_Quaterniond(Eigen::eigen2_AngleAxisd(gaussianRand(0, M_PI / 8) * amount, Eigen::Vector3d::UnitX())); break;
				case 4: rot *= Eigen::eigen2_Quaterniond(Eigen::eigen2_AngleAxisd(gaussianRand(0, M_PI / 8) * amount, Eigen::Vector3d::UnitY())); break;
				case 5: rot *= Eigen::eigen2_Quaterniond(Eigen::eigen2_AngleAxisd(gaussianRand(0, M_PI / 8) * amount, Eigen::Vector3d::UnitZ())); break;
				default: break;
			};
		}
		/*
		tr.x() += gaussianRand(0, 0.1) * amount;
		tr.y() += gaussianRand(0, 0.1) * amount;
		tr.z() += gaussianRand(0, 0.1) * amount;
		rot *= Eigen::eigen2_Quaterniond(Eigen::AngleAxisd(gaussianRand(0, M_PI / 8) * amount, Eigen::Vector3d::UnitX()));
		rot *= Eigen::eigen2_Quaterniond(Eigen::AngleAxisd(gaussianRand(0, M_PI / 8) * amount, Eigen::Vector3d::UnitY()));
		rot *= Eigen::eigen2_Quaterniond(Eigen::AngleAxisd(gaussianRand(0, M_PI / 8) * amount, Eigen::Vector3d::UnitZ()));
		*/
		return *this;
	}
	
	void dump(ostream& stream) const
	{
		/*stream <<
			"translation: x=" << tr.x() << " y=" << tr.y() << " z=" << tr.z() << " " <<
			"quaternion: x=" << rot.x() << " y=" << rot.y() << " z=" << rot.z() << " w=" << rot.w();
		*/
		stream << "<node pkg=\"tf\" type=\"static_transform_publisher\" name=\"base_link_to_kinect\""
				" args=\"" << tr.x() << " " << tr.y() << " " << tr.z() << " " <<
				rot.x() << " " << rot.y() << " " << rot.z() << " " << rot.w() << " "
				"/base_link /kinect 100\"/>" 
        " \n\n OR \n\n"
        "rosrun tf static_transform_publisher " << tr.x() << " " << tr.y() << " " << tr.z() << " " <<
        rot.x() << " " << rot.y() << " " << rot.z() << " " << rot.w() << " "
        "/base_link /kinect 100";
	}
};

typedef vector<Params> Genome;

double computeError(const Params& p, const TrainingEntry& e)
{
	/*
	// version with Eigen::Matrix4d
	Eigen::Matrix4d blk(Eigen::Matrix4d::Identity());
	blk.corner(Eigen::TopLeft,3,3) = p.rot.toRotationMatrix();
	blk.corner(Eigen::TopRight,3,1) = p.tr;
	
	Eigen::Matrix4d odom(Eigen::Matrix4d::Identity());
	odom.corner(Eigen::TopLeft,3,3) = e.odom_rot.toRotationMatrix();
	odom.corner(Eigen::TopRight,3,1) = e.odom_tr;
	
	Eigen::Matrix4d blk_i(blk.inverse());
	
	//const Eigen::Matrix4d pred_icp = blk * odom * blk_i;
	const Eigen::Matrix4d pred_icp = blk_i * odom * blk;
	
	const Eigen::Matrix3d pred_icp_rot_m = pred_icp.corner(Eigen::TopLeft,3,3);
	const Eigen::eigen2_Quaterniond pred_icp_rot = Eigen::eigen2_Quaterniond(pred_icp_rot_m);
	const Eigen::Vector3d pred_icp_tr = pred_icp.corner(Eigen::TopRight,3,1);
	*/
	
	// version with Eigen::Transform3d
	const Eigen::eigen2_Transform3d blk = Eigen::eigen2_Translation3d(p.tr) * p.rot;
	const Eigen::eigen2_Transform3d blk_i = Eigen::eigen2_Transform3d(blk.inverse(Eigen::Isometry));
	const Eigen::eigen2_Transform3d odom = Eigen::eigen2_Translation3d(e.odom_tr) * e.odom_rot;
	//const Eigen::Transform3d pred_icp = blk * odom * blk_i;
	const Eigen::eigen2_Transform3d pred_icp = blk_i * odom * blk;
	
	const Eigen::Matrix3d pred_icp_rot_m = pred_icp.matrix().topLeftCorner(3,3);
	const Eigen::eigen2_Quaterniond pred_icp_rot = Eigen::eigen2_Quaterniond(pred_icp_rot_m);
	const Eigen::Vector3d pred_icp_tr = pred_icp.translation();
	
	
	// identity checked ok
	//cerr << "mat:\n" << blk.matrix() * blk_i.matrix() << endl;
	
	//cout << "dist: " << (e.icp_tr - pred_icp.translation()).norm() << endl;
	//cout << "ang dist: " << e.icp_rot.angularDistance(pred_icp_rot) << endl;
	//cout << "tr pred icp:\n" << pred_icp.translation() << "\nicp:\n" << e.icp_tr << "\n" << endl;
	//cout << "rot pred icp:\n" << pred_icp_rot << "\nicp:\n" << e.icp_rot << "\n" << endl;
	// FIXME: tune coefficient for rot vs trans
	
	const double e_tr((e.icp_tr - pred_icp_tr).norm());
	const double e_rot(e.icp_rot.angularDistance(pred_icp_rot));
	/*if (e_tr < 0)
		abort();
	if (e_rot < 0)
		abort();*/
	//cerr << e_tr << " " << e_rot << endl;
	return e_tr + e_rot;
}

double computeError(const Params& p)
{
	double error = 0;
	for (TrainingSet::const_iterator it(trainingSet.begin()); it != trainingSet.end(); ++it)
	{
		const TrainingEntry& entry(*it);
		error += computeError(p, entry);
	}
	return error;
}

double evolveOneGen(Genome& genome, double annealing = 1.0, bool showBest = false)
{
	typedef multimap<double, Params> EvaluationMap;
	typedef EvaluationMap::iterator EvaluationMapIterator;
	EvaluationMap evalutationMap;

	double totalError = 0;
	double bestError = numeric_limits<double>::max();
	int bestInd = 0;
	for (size_t ind = 0; ind < genome.size(); ind++)
	{
		const double error = computeError(genome[ind]);
		if (error < bestError)
		{
			bestError = error;
			bestInd = ind;
		}

		totalError += error;
		evalutationMap.insert(make_pair(error, genome[ind]));

		/*cout << "E " << ind << " : ";
		genome[ind].dump(cout);
		cout << " = " << error << "\n";*/
	}

	if (showBest)
	{
		//cout << "Best of gen: ";
		genome[bestInd].dump(cout);
		cout << endl;
	}

	assert((genome.size() / 4) * 4 == genome.size());

	size_t ind = 0;
	for (EvaluationMapIterator it = evalutationMap.begin(); ind < genome.size() / 4; ++it, ++ind )
	{
		//cout << "S " << it->first << "\n";
		genome[ind * 4] = it->second;
		genome[ind * 4 + 1] = it->second.mutate(annealing);
		genome[ind * 4 + 2] = it->second.mutate(annealing);
		genome[ind * 4 + 3] = it->second.mutate(annealing);
	}

	return bestError;
}

int main(int argc, char** argv)
{
	srand(time(0));
	
	if (argc != 2)
	{
		cerr << "Usage " << argv[0] << " LOG_FILE_NAME" << endl;
		return 1;
	}
	
	ifstream ifs(argv[1]);
	while (ifs.good())
	{
		TrainingEntry e(ifs);
		if (ifs.good())
		{
			trainingSet.push_back(e);
//			e.dump(cerr); cerr << endl;
		}
	}
	cout << "Loaded " << trainingSet.size() << " training entries" << endl;
	//trainingSet.dump();
	
	Genome genome(1024);
	//Genome genome(4);
	int generationCount = 64;
	for (int i = 0; i < generationCount; i++)
	{
		cout << i << " best has error " << evolveOneGen(genome, 2. * (double)(generationCount - i) / (double)(generationCount)) << endl;
	}
	/*generationCount = 200;
	for (int i = 0; i < generationCount; i++)
	{
		cout << 50 + i << " best has error " << evolve_one_gen(genome, 1.) << endl;
	}*/
	cout << "\nOptimization completed, code to COPY-PASTE in to use the transformation:\n\n";
	evolveOneGen(genome, 1.0, true);
	cout << endl;
	
	return 0;
}

#include <iostream>

#include "QuickDraw.h"

#include "Room.h"
#include "Ship.h"
#include "Bullet.h"
using namespace std;
Obstacle::Obstacle (int xa, int ya, int rad) : x1(xa), y1(ya), radius(rad)
{
}

Obstacle::~Obstacle ()
{
}

void Obstacle::display (View & view, double offsetx, double offsety, double scale)
{
	// Find center of screen.
	int cx, cy;
	view.screenSize (cx, cy);
	cx = cx / 2;
	cy = cy / 2;

	int xa = (int) ((x1 - (offsetx - cx)) * scale);
	int ya = (int) ((y1 - (offsety - cy)) * scale);
	int rad = (int) (radius * scale);

	view.drawSolidCircle (xa, ya, rad, 64, 64, 64);
}

// Perform line segment/sphere intersection testing.
bool Obstacle::collides (double xa, double ya, double xb, double yb)
{
	// let V be the vector from A to B
	double vx = xb - xa;
	double vy = yb - ya;

	// Let W be the vector from A to the center of the sphere.
	double wx = x1 - xa;
	double wy = y1 - ya;

	// Distance of center from the line is scalar projection of W onto the normal to the line.
	// = dot product of W and unit v normal.
	double vlen = sqrt (vx * vx + vy * vy);
	double vnormalx = -vy / vlen;
	double vnormaly = vx / vlen;

	double distance = abs (wx * vnormalx + wy * vnormaly);

	if (distance <= radius)
	{
		// Sphere overlaps the line somewhere.
		// Now check to see if it touches between A and B.
		// Get the scalar projection of W onto V. If this is between 0 and |V|, then the intersection is between A and B.
		double between = (wx * vx + wy * vy) / vlen;

		// The sphere center could project beyond the endpoints but still overlap between A and B. The extra amount depends on
		// how close the sphere is to the line.
		double extra = sqrt ((radius * radius) - ((radius - distance) * (radius - distance)));
		if ((between > - extra) && (between < vlen + extra))
		{
			return true;
		}
	}

	return false;
}

Room::Room(int l, int r, int t, int b) : left (l), right (r), top (t), bottom (b)
{
	srand (199);
	int lr = right - left;
	int tb = top - bottom;
	for (int i = 0; i < NUMOBSTACLES; i++)
	{
		obstacles.push_back (new Obstacle (rand () % (lr) + left, rand () % tb + bottom, rand () % (lr / 10)));
	}
}

Room::~Room(void)
{
	for (std::vector <Obstacle *>::iterator i = obstacles.begin (); i != obstacles.end (); i++)
	{
		delete (*i);
	}
	for (std::vector <Actor *>::iterator i = actors.begin (); i != actors.end (); i++)
	{
		delete (*i);
	}
}

void Room::update (double deltat)
{
	// Move the rats.
	double avgratx = 0.0;
	double avgraty = 0.0;

	// Avoid using iterators since the list may grow during updates.
	for (unsigned int i = 0; i < actors.size (); )
	{
		if (!(actors[i])->update (*this, deltat))
		{
			delete (actors[i]);
			actors.erase (actors.begin () + i);
		}
		else
		{
			// Check for collisions between objects.
			bool killi = false;
			for (unsigned int j = 0; j < actors.size (); )
			{
				bool killj = false;
				double rx, ry;
				double bx, by;
				(actors[i])->getPosition (rx, ry);
				(actors[j])->getPosition (bx, by);
				if ((i != j) && (fabs (rx - bx) + fabs (ry - by) < actors[i]->getRadius () + actors[j]->getRadius ()))
				{
					// two objects are colliding.
					if ((actors[j]->getType () == Actor::BULLET) &&
						(actors[i]->getType () == Actor::SHIP))
					{
						if (((Ship*) (actors[i]))->isFairGame ())
						{
							// remove bullet, kill ship.
							killj = true;
							killi = true;
						}
					}
				}

				if (killj)
				{
					((Ship *) (((Bullet*) (actors[j]))->getOwner ()))->addHit ();
					// remove the bullet.
					delete (actors[j]);
					actors.erase (actors.begin () + j);
				}
				else
				{
					j++;
				}
			}

			if (killi)
			{
				((Ship*) (actors[i]))->triggerKill ();
			}
			else
			{
				i++;
			}
		}
	}
}

void Room::display (View & view, double offsetx, double offsety, double scale)

{
	// Find center of screen.
	int cx, cy;
	view.screenSize (cx, cy);
	cx = cx / 2;
	cy = cy / 2;

	int xl = (int) ((left - (offsetx - cx)) * scale);
	int xr = (int) ((right - (offsetx - cx)) * scale);
	int yt = (int) ((top - (offsety - cy)) * scale);
	int yb = (int) ((bottom - (offsety - cy)) * scale);

	view.drawLine (xl, yt, xl, yb, 255, 0, 0);
	view.drawLine (xr, yt, xr, yb, 255, 0, 0);
	view.drawLine (xl, yt, xr, yt, 255, 0, 0);
	view.drawLine (xl, yb, xr, yb, 255, 0, 0);

	for (std::vector <Obstacle *>::iterator i = obstacles.begin (); i != obstacles.end (); i++)
	{
		(*i)->display (view, offsetx, offsety, scale);
	}
	for (std::vector <Actor *>::iterator i = actors.begin (); i != actors.end (); i++)
	{
		(*i)->display (view, offsetx, offsety, scale);
	}
}

bool Room::canMove (double x1, double y1, double x2, double y2)
{
	if ((x2 < left) || (x2 > right) || (y2 > top) || (y2 < bottom))
		return false;

	for (std::vector <Obstacle *>::iterator i = obstacles.begin (); i != obstacles.end (); i++)
	{
		// False if collides with any single Obstacle.
		if ((*i)->collides (x1, y1, x2, y2))
			return false;
	}
	return true;
}

void Room::addActor (Actor * actor)
{
	actors.push_back (actor);
}

const std::vector <Actor *> Room::getActors ()

{
	return actors;
}

// i made dis, i.imgur.com/kNGKIvW.jpg
char * Room::serialize(int code, int & size)
{
	//+--------+--------+--------+
	//|x-pos   |y-pos   |d-direc |
	//+--------+--------+--------+

	int elementsize = sizeof(double) + sizeof(double) + sizeof(double); // =24
	size = sizeof(int) + elementsize * actors.size(); // =100
	char * data = new char[size];
	*(int *)data = code;
	int count = 0; //dis is so i know which part of the packet to go into.  
	for each (Ship *s in actors)
	{
		//dese are the variables
		double x = 0;
		double y = 0;
		double d = 0;
		//dis is how i get em
		s->getPosition(x, y);
		s->getDirection(d);
		//dis is how i put em in the packet
		(*(double*)(data + sizeof(int) + count * elementsize)) = x;
		(*(double*)(data + sizeof(int) + count * elementsize + sizeof(double))) = y;
		(*(double*)(data + sizeof(int) + count * elementsize + sizeof(double) + sizeof(double))) = d;
		std::cout << "Position: " << x << ", " << y << ", Direction: " << d << std::endl;
		count++;
	}
	std::cout << "=======================================\n";
	std::cout << sizeof(data) << std::endl;						
	//send dat packet
	return data;
}

// i really dont know what im doing lel
void Room::deserialize(char * data, int size)
{
	int elementsize = sizeof(double) + sizeof(double) + sizeof(double); // =24
	int count = 0;
	//take the data from the packets and put it to the actors.
	for each (Ship *s in actors)
	{
		double x = 0;
		double y = 0;
		double d = 0;
		x = (*(double*)(data + sizeof(int) + count * elementsize));
		y = (*(double*)(data + sizeof(int) + count * elementsize + sizeof(double)));
		d = (*(double*)(data + sizeof(int) + count * elementsize + sizeof(double) + sizeof(double)));
		s->setPosition(x, y);
		s->setDirection(d);
		count++;
	}
}

void Room::updatePlayerStuff(double x, double y, double d)
{
	actors.front()->setPosition(x, y);
}
//This plugin loads an .stl file (binary files only)
//The stl loader that is included in C4D has problems opening some files. But this code seem to open them fine
//The stl parsing code is based on this code: https://github.com/dillonhuff/stl_parser
//NOTE: There is a small memory leak coming from parse_stl() per import that I can't squash!!

#include "c4d.h"
#include "c4d_symbols.h"
#include "ge_dynamicarray.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

#define PLUGIN_ID 1040842


struct point
{
    float x;
    float y;
    float z;

    point() : x(0), y(0), z(0) {}
    point(float xp, float yp, float zp) : x(xp), y(yp), z(zp) {}
};

struct triangle
{
    point normal;
    point v1;
    point v2;
    point v3;
    //triangle(point normalp, point v1p, point v2p, point v3p) : normal(normalp), v1(v1p), v2(v2p), v3(v3p) {}

    //Flip the normals to work with C4D's Left handed coords
    triangle(point normalp, point v1p, point v3p, point v2p) : normal(normalp), v1(v1p), v2(v2p), v3(v3p) {}
};

struct stl_data
{
    std::string name;
    std::vector<triangle> triangles;

    stl_data(std::string namep) : name(namep) {}
};

float parse_float(std::ifstream &s)
{
    char f_buf[sizeof(float)];
    s.read(f_buf, 4);
    float *fptr = (float*)f_buf;
    return *fptr;
}

point parse_point(std::ifstream &s)
{
    float x = parse_float(s);
    float y = parse_float(s);
    float z = parse_float(s);
    //return point(x, y, z);
    return point(x, z, y);     //Flip the Y & Z to work with C4D's Left handed coords
}

stl_data parse_stl(const std::string &stl_path)
{
    std::ifstream stl_file(stl_path.c_str(), std::ios::in | std::ios::binary);
    if (!stl_file)
    {
        //std::cout << "ERROR: COULD NOT READ FILE" << std::endl;
        GePrint("ERROR: COULD NOT READ FILE");
    }
    char header_info[80] = "";
    char n_triangles[4];
    stl_file.read(header_info, 80);
    stl_file.read(n_triangles, 4);
    std::string h(header_info);
    stl_data info(h);
    unsigned int *r = (unsigned int*)n_triangles;
    unsigned int num_triangles = *r;
    for (unsigned int i = 0; i < num_triangles; i++)
    {
        point normal = parse_point(stl_file);
        auto v1 = parse_point(stl_file);
        auto v2 = parse_point(stl_file);
        auto v3 = parse_point(stl_file);
        info.triangles.push_back(triangle(normal, v1, v2, v3));
        char dummy[2];
        stl_file.read(dummy, 2);
    }

    return info;
}



class SimplePlugin : public CommandData
{
public:
    SimplePlugin();
    virtual Bool    Execute(BaseDocument *doc);
};

SimplePlugin::SimplePlugin() // The Constructor
{
    //not used in this example
}

Bool SimplePlugin::Execute(BaseDocument *doc)
{
    //std::cout << "STL HEADER = " << info.name << std::endl;
    //std::cout << "# triangles = " << triangles.size() << std::endl;    
    //GePrint(LongToString(triangles.size()));

    //Load the file with a file browser
    Filename file;
    file.FileSelect(FILESELECTTYPE_SCENES, FILESELECT_LOAD, "Load .stl File");    

    if (file.CheckSuffix("STL"))
    {
        //Before we parse the stl file we must first check if it's binary or Ascii
        //Because an Ascii file will crash this plugin!!!
        AutoAlloc<BaseFile> bf;
        if (!bf->Open(file, FILEOPEN_READ, FILEDIALOG_ANY, BYTEORDER_INTEL, MACTYPE_CINEMA, MACCREATOR_CINEMA)) return false;

        //Store the text from the text file in this variable
        String text = "";

        VLONG fileLength = bf->GetLength();
        CHAR c[2];
        c[1] = 0;
        CHAR *pc = &c[0];
        for (LONG i = 0L; i != fileLength; ++i)
        {
            bf->ReadChar(pc);      //Line of text read from the file 
            text += String(pc);    //Gets the next line of text        
            if (i>20) break;       //No need to read the entire file
        }
        //GePrint(text);
        bool isAscii = text.FindFirst("solid", 0);
        bf->Close();

        //Parse the stl file if it's binary
        if (!isAscii)
        {
            auto info = parse_stl(file.GetFileString().GetCStringCopy());
 
            //Get the name of the file so we can use it as the object's name later on    
            file.ClearSuffix();
            String name = file.GetFileString();

            std::vector<triangle> triangles = info.triangles;

            LONG triCount = triangles.size();
            LONG pntCount = triCount * 3;

            PolygonObject *finalPoly = PolygonObject::Alloc(triCount * 3, triCount);
            Vector *points = finalPoly->GetPointW();
            CPolygon *polys = finalPoly->GetPolygonW();

            Vector posa;
            Vector posb;
            Vector posc;

            //Create an array of point positions for the triangles
            GeDynamicArray<Vector>tri_positions;

            for (LONG i = 0; i < pntCount; i++)
            {
                LONG a = i * 3;
                LONG b = i * 3 + 1;
                LONG c = i * 3 + 2;

                if (i < triCount)
                {
                    //The three point positions in each triangle
                    posa = Vector(triangles[i].v1.x, triangles[i].v1.y, triangles[i].v1.z);
                    posb = Vector(triangles[i].v2.x, triangles[i].v2.y, triangles[i].v2.z);
                    posc = Vector(triangles[i].v3.x, triangles[i].v3.y, triangles[i].v3.z);
                    //GePrint(RealToString(posa.x) + " " + RealToString(posa.y) + " " + RealToString(posa.z));
                    //GePrint(RealToString(posb.x) + " " + RealToString(posb.y) + " " + RealToString(posb.z));
                    //GePrint(RealToString(posc.x) + " " + RealToString(posc.y) + " " + RealToString(posc.z));

                    tri_positions.Push(posa);
                    tri_positions.Push(posb);
                    tri_positions.Push(posc);

                    //Create the polygon faces
                    if (i < triCount) polys[i] = CPolygon(a, b, c);
                }

                //Set the point positions
                points[i] = tri_positions[i];
            }

            finalPoly->Message(MSG_UPDATE);
            doc->InsertObject(finalPoly, NULL, NULL);

            //Weld the common points using the optimize command
            BaseContainer bc;
            ModelingCommandData mdat;
            mdat.doc = doc;
            mdat.op = finalPoly;
            mdat.bc = &bc;
            bc.SetBool(MDATA_OPTIMIZE_POINTS, TRUE);
            bc.SetBool(MDATA_OPTIMIZE_POLYGONS, TRUE);
            bc.SetReal(MDATA_OPTIMIZE_TOLERANCE, 0.5);
            SendModelingCommand(MCOMMAND_OPTIMIZE, mdat);

            finalPoly = nullptr;


            //Still not killing memory leaks!!?
            info.triangles.clear();
            info.name.clear();           
            pc = nullptr;
            GeFree(pc);
            delete pc;
            tri_positions.ReSize(0);
            tri_positions.FreeArray();
        }

        else GeOutString("Ascii stl Files Not Supported!", GEMB_ICONEXCLAMATION);       
    }
    
    EventAdd();
    return true;
}


Bool RegisterStlImporter()
{
    return RegisterCommandPlugin(PLUGIN_ID, GeLoadString(IDS_My_Simple_Plugin), 0, AutoBitmap("stl_icon.png"), String("STL Importer"), gNew SimplePlugin);
}
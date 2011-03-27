/*
 * shaders.cpp
 *
 *  Created on: 2010-04-24
 *      Author: krzysztof marczak
 */

#include <cstdlib>

#include "shaders.h"

sLight *Lights;
int lightsPlaced = 0;

sShaderOutput MainShadow(sParamRender *param, sFractal *calcParam, CVector3 point, CVector3 lightVect, double wsp_persp, double dist_thresh)
{
	sShaderOutput shadow = { 1.0, 1.0, 1.0 };

	//starting point
	CVector3 point2;

	bool max_iter;
	double factor = 1.0 * param->doubles.zoom * wsp_persp;
	double dist = dist_thresh;

	double start = dist_thresh;
	if(calcParam->interiorMode) 
		start = dist_thresh*param->doubles.DE_factor*0.5;

	for (double i = start; i < factor; i += dist * param->doubles.DE_factor)
	{
		point2 = point + lightVect * i;

		dist = CalculateDistance(point2, *calcParam, &max_iter);
		if (param->fractal.iterThresh)
		{
			if (dist < dist_thresh && !max_iter)
			{
				dist = dist_thresh * 1.01;
			}
		}
		if (dist < dist_thresh || max_iter)
		{
			double shadowing = i / factor;
			shadow.R = shadowing;
			shadow.G = shadowing;
			shadow.B = shadowing;
			break;
		}

	}
	return shadow;
}

sShaderOutput AmbientOcclusion(sParamRender *param, sFractal *calcParam, CVector3 point, double wsp_persp, double dist_thresh, double last_distance, sVectorsAround *vectorsAround,
		int vectorsCount, CVector3 normal)
{
	sShaderOutput AO = { 0, 0, 0 };

	double delta = param->doubles.resolution * param->doubles.zoom * wsp_persp;

	bool max_iter;
	double start_dist = dist_thresh;
	double end_dist = param->doubles.zoom * wsp_persp;
	double intense = 0;
	if (start_dist < delta) start_dist = delta;
	for (int i = 0; i < vectorsCount; i++)
	{
		sVectorsAround v = vectorsAround[i];

		double dist = last_distance;
		bool not_shade = true;

		//double shade = normal.Dot(vectorsAround[i].v);
		//if(shade<0) shade = 0;

		for (double r = start_dist; r < end_dist; r += dist * 2.0)
		{
			CVector3 point2 = point + v.v * r;

			dist = CalculateDistance(point2, *calcParam, &max_iter);
			if (param->fractal.iterThresh)
			{
				if (dist < dist_thresh && !max_iter)
				{
					dist = dist_thresh * 1.01;
				}
			}
			if (dist < dist_thresh || max_iter)
			{
				not_shade = false;
				intense = r / end_dist;
				break;
			}
		}
		if (not_shade)
		{
			AO.R += v.R;
			AO.G += v.G;
			AO.B += v.B;
		}
		else
		{
			AO.R += intense * v.R;
			AO.G += intense * v.G;
			AO.B += intense * v.B;
		}
	}
	AO.R /= (vectorsCount * 256.0);
	AO.G /= (vectorsCount * 256.0);
	AO.B /= (vectorsCount * 256.0);

	return AO;
}

CVector3 CalculateNormals(sParamRender *param, sFractal *calcParam, CVector3 point, double dist_thresh)
{
	CVector3 normal(0, 0, 0);
	//calculating normal vector based on distance estimation (gradient of distance function)
	if (!param->slowShading)
	{
		//calcParam->DE_threshold = 0;
		//double delta = param->resolution * param->zoom * wsp_persp;
		double delta = dist_thresh * param->doubles.smoothness;
		if(calcParam->interiorMode) delta = dist_thresh * 0.2;

		double s1, s2, s3, s4;
		calcParam->N = param->fractal.N * 2;
		calcParam->minN = 0;

		s1 = CalculateDistance(point, *calcParam);

		CVector3 deltax(delta, 0.0, 0.0);
		s2 = CalculateDistance(point + deltax, *calcParam);

		CVector3 deltay(0.0, delta, 0.0);
		s3 = CalculateDistance(point + deltay, *calcParam);

		CVector3 deltaz(0.0, 0.0, delta);
		s4 = CalculateDistance(point + deltaz, *calcParam);

		normal.x = s2 - s1;
		normal.y = s3 - s1;
		normal.z = s4 - s1;
		calcParam->N = param->fractal.N;
		calcParam->minN = param->fractal.minN;
		//calcParam->DE_threshold = DEthreshold_temp;
	}

	//calculating normal vector based on average value of binary central difference
	else
	{
		double result2;
		bool max_iter;

		CVector3 point2;
		CVector3 point3;
		for (point2.x = -1.0; point2.x <= 1.0; point2.x += 0.2) //+0.2
		{
			for (point2.y = -1.0; point2.y <= 1.0; point2.y += 0.2)
			{
				for (point2.z = -1.0; point2.z <= 1.0; point2.z += 0.2)
				{
					point3 = point + point2 * dist_thresh * param->doubles.smoothness;

					double dist = CalculateDistance(point3, *calcParam, &max_iter);

					if (param->fractal.iterThresh)
					{
						if (dist < dist_thresh && !max_iter)
						{
							dist = dist_thresh * 1.01;
						}
					}
					if (dist < dist_thresh || max_iter) result2 = 0.0;
					else result2 = 1.0;

					normal += (point2 * result2);
				}
			}
		}
	}
	normal.Normalize();

	return normal;
}

sShaderOutput MainShading(CVector3 normal, CVector3 lightVector)
{
	sShaderOutput shading;
	double shade = normal.Dot(lightVector);
	if (shade < 0) shade = 0;
	shading.R = shade;
	shading.G = shade;
	shading.B = shade;
	return shading;
}

sShaderOutput MainSpecular(CVector3 normal, CVector3 lightVector, CVector3 viewVector)
{
	sShaderOutput specular;
	CVector3 half = lightVector - viewVector;
	half.Normalize();
	double shade2 = normal.Dot(half);
	if (shade2 < 0.0) shade2 = 0.0;
	shade2 = pow(shade2, 30.0) * 1.0;
	if (shade2 > 15.0) shade2 = 15.0;
	specular.R = shade2;
	specular.G = shade2;
	specular.B = shade2;
	return specular;
}

sShaderOutput EnvMapping(CVector3 normal, CVector3 viewVector, cTexture *texture)
{
	sShaderOutput envReflect;
	CVector3 reflect;
	double dot = -viewVector.Dot(normal);
	reflect = normal * 2.0 * dot + viewVector;

	double alfaTexture = reflect.GetAlfa() + M_PI;
	double betaTexture = reflect.GetBeta();
	double texWidth = texture->Width();
	double texHeight = texture->Height();

	if (betaTexture > 0.5 * M_PI) betaTexture = 0.5 * M_PI - betaTexture;

	if (betaTexture < -0.5 * M_PI) betaTexture = -0.5 * M_PI + betaTexture;

	double dtx = (alfaTexture / (2.0 * M_PI)) * texWidth + texWidth * 8.25;
	double dty = (betaTexture / (M_PI) + 0.5) * texHeight + texHeight * 8.0;
	dtx = fmod(dtx, texWidth);
	dty = fmod(dty, texHeight);
	if (dtx < 0) dtx = 0;
	if (dty < 0) dty = 0;
	envReflect.R = texture->Pixel(dtx, dty).R / 256.0;
	envReflect.G = texture->Pixel(dtx, dty).G / 256.0;
	envReflect.B = texture->Pixel(dtx, dty).B / 256.0;
	return envReflect;
}

sShaderOutput LightShading(sParamRender *fractParams, sFractal *calcParam, CVector3 point, CVector3 normal, CVector3 viewVector, sLight light, double wsp_persp,
		double dist_thresh, int number, sShaderOutput *outSpecular, bool accurate)
{
	sShaderOutput shading;

	CVector3 d = light.position - point;

	double distance = d.Length();

	//angle of incidence
	CVector3 lightVector = d;
	lightVector.Normalize();

	double intensity = fractParams->doubles.auxLightIntensity * 100.0 * light.intensity / (distance * distance) / number;
	double shade = normal.Dot(lightVector);
	if (shade < 0) shade = 0;
	shade = shade * intensity;
	if (shade > 5.0) shade = 5.0;

	//specular
	CVector3 half = lightVector - viewVector;
	half.Normalize();
	double shade2 = normal.Dot(half);
	if (shade2 < 0.0) shade2 = 0.0;
	shade2 = pow(shade2, 30.0) * 1.0;
	shade2 *= intensity;
	if (shade2 > 15.0) shade2 = 15.0;

	//calculate shadow
	if (shade > 0.01 || shade2 > 0.01)
	{
		double delta = fractParams->doubles.resolution * fractParams->doubles.zoom * wsp_persp;
		double stepFactor = 1.0;
		double step = delta * fractParams->doubles.DE_factor * stepFactor;
		double dist = step;
		for (double i = dist_thresh; i < distance; i += dist * stepFactor * fractParams->doubles.DE_factor)
		{
			CVector3 point2 = point + lightVector * i;

			bool max_iter;
			dist = CalculateDistance(point2, *calcParam, &max_iter);
			if (fractParams->fractal.iterThresh)
			{
				if (dist < dist_thresh && !max_iter)
				{
					dist = dist_thresh * 1.01;
				}
			}
			if (dist < 0.5 * dist_thresh || max_iter)
			{
				shade = 0;
				shade2 = 0;
				break;
			}
		}

	}
	else
	{
		shade = 0;
		shade2 = 0;
	}

	shading.R = shade * light.colour.R / 65536.0;
	shading.G = shade * light.colour.G / 65536.0;
	shading.B = shade * light.colour.B / 65536.0;

	outSpecular->R = shade2 * light.colour.R / 65536.0;
	outSpecular->G = shade2 * light.colour.G / 65536.0;
	outSpecular->B = shade2 * light.colour.B / 65536.0;

	return shading;
}

void PlaceRandomLights(sParamRender *fractParams, bool onlyPredefined)
{
	srand(fractParams->auxLightRandomSeed);

	if(!onlyPredefined)
	{
		delete[] Lights;
		Lights = new sLight[fractParams->auxLightNumber + 4];
	}
	sFractal calcParam = fractParams->fractal;
	bool max_iter;

	int trial_number = 0;
	double radius_multiplier = 1.0;


	if(!onlyPredefined)
	{
	for (int i = 0; i < fractParams->auxLightNumber; i++)
		{
			trial_number++;

			CVector3 random;
			random.x = (Random(2000000) / 1000000.0 - 1.0) + (Random(1000000) / 1.0e12);
			random.y = (Random(2000000) / 1000000.0 - 1.0) + (Random(1000000) / 1.0e12);
			random.z = (Random(2000000) / 1000000.0 - 1.0) + (Random(1000000) / 1.0e12);

			CVector3 position = fractParams->doubles.auxLightRandomCenter + random * fractParams->doubles.auxLightDistributionRadius * radius_multiplier;

			double distance = CalculateDistance(position, calcParam, &max_iter);

			if (trial_number > 1000)
			{
				radius_multiplier *= 1.1;
				trial_number = 0;
			}

			if (distance > 0 && !max_iter && distance < fractParams->doubles.auxLightMaxDist * radius_multiplier)
			{
				radius_multiplier = 1.0;

				Lights[i].position = position;

				sRGB colour = { 20000 + Random(80000), 20000 + Random(80000), 20000 + Random(80000) };
				double maxColour = dMax(colour.R, colour.G, colour.B);
				colour.R = colour.R * 65536.0 / maxColour;
				colour.G = colour.G * 65536.0 / maxColour;
				colour.B = colour.B * 65536.0 / maxColour;
				Lights[i].colour = colour;

				Lights[i].intensity = distance * distance / fractParams->doubles.auxLightMaxDist;
				Lights[i].enabled = true;
				printf("Light no. %d: x=%f, y=%f, z=%f, distance=%f\n", i, position.x, position.y, position.z, distance);
			}
			else
			{
				i--;
			}
		}
	}

	if (fractParams->auxLightPre1Enabled)
	{
		Lights[0].position = fractParams->doubles.auxLightPre1;
		Lights[0].intensity = fractParams->doubles.auxLightPre1intensity;
		Lights[0].colour = fractParams->auxLightPre1Colour;
		Lights[0].enabled = true;
	}
	else
	{
		Lights[0].enabled = false;
	}

	if (fractParams->auxLightPre2Enabled)
	{
		Lights[1].position = fractParams->doubles.auxLightPre2;
		Lights[1].intensity = fractParams->doubles.auxLightPre2intensity;
		Lights[1].colour = fractParams->auxLightPre2Colour;
		Lights[1].enabled = true;
	}
	else
	{
		Lights[1].enabled = false;
	}

	if (fractParams->auxLightPre3Enabled)
	{
		Lights[2].position = fractParams->doubles.auxLightPre3;
		Lights[2].intensity = fractParams->doubles.auxLightPre3intensity;
		Lights[2].colour = fractParams->auxLightPre3Colour;
		Lights[2].enabled = true;
	}
	else
	{
		Lights[2].enabled = false;
	}

	if (fractParams->auxLightPre4Enabled)
	{
		Lights[3].position = fractParams->doubles.auxLightPre4;
		Lights[3].intensity = fractParams->doubles.auxLightPre4intensity;
		Lights[3].colour = fractParams->auxLightPre4Colour;
		Lights[3].enabled = true;
	}
	else
	{
		Lights[3].enabled = false;
	}

	lightsPlaced = fractParams->auxLightNumber;
	if (lightsPlaced < 4) lightsPlaced = 4;
}

void PostRenderingLights(cImage *image, sParamRender *fractParam)
{
	int width = image->GetWidth();
	int height = image->GetHeight();
	double aspectRatio = (double) width / height;

	//preparing rotation matrix
	CRotationMatrix mRot;
	mRot.RotateY(-fractParam->doubles.gamma);
	mRot.RotateX(-fractParam->doubles.beta);
	mRot.RotateZ(-fractParam->doubles.alfa);

	CVector3 point3D1, point3D2;

	int numberOfLights = lightsPlaced;

	if (numberOfLights < 4) numberOfLights = 4;

	for (int i = 0; i < numberOfLights; ++i)
	{
		if (i < fractParam->auxLightNumber || Lights[i].enabled)
		{
			point3D1 = Lights[i].position - fractParam->doubles.vp;
			point3D2 = mRot.RotateVector(point3D1);
			double y2 = point3D2.y;
			double y = y2 / fractParam->doubles.zoom;
			double wsp_persp = 1.0 + y * fractParam->doubles.persp;
			double x2 = point3D2.x / wsp_persp;
			double z2 = point3D2.z / wsp_persp;
			double x = (x2 / (fractParam->doubles.zoom * aspectRatio) + 0.5) * width;
			double z = (z2 / fractParam->doubles.zoom + 0.5) * height;

			int xs = (int) x;
			int ys = (int) z;

			if (xs >= 0 && xs < width && ys >= 0 && ys < height)
			{

				if (y < image->GetPixelZBuffer(xs,ys) && y > (-1.0 / fractParam->doubles.persp))
				{
					int R = Lights[i].colour.R;
					int G = Lights[i].colour.G;
					int B = Lights[i].colour.B;

					double size = 50.0 / wsp_persp * Lights[i].intensity / fractParam->doubles.zoom * width * fractParam->doubles.auxLightIntensity / fractParam->auxLightNumber
							* fractParam->doubles.auxLightVisibility;

					int x_start = xs - size * 5 - 1;
					int x_end = xs + size * 5 + 1;
					int y_start = ys - size * 5 - 1;
					int y_end = ys + size * 5 + 1;

					if (x_start < 0) x_start = 0;
					if (x_end >= width - 1) x_end = width - 1;
					if (y_start < 0) y_start = 0;
					if (y_end >= height - 1) y_end = height - 1;

					for (int yy = y_start; yy <= y_end; yy++)
					{
						for (int xx = x_start; xx <= x_end; xx++)
						{
							double dx = xx - x;
							double dy = yy - z;
							double r = sqrt(dx * dx + dy * dy) / size;

							double r2 = sqrt(dx * dx + dy * dy);
							if (r2 > size * 5) r2 = size * 5;
							double bellFunction = (cos(r2 * M_PI/(size*5.0)) + 1.0) / 2.0;

							double bright = bellFunction / (r * r);
							if (bright > 10.0) bright = 10.0;

							sRGB16 oldPixel = image->GetPixelImage(xx,yy);
							int oldAlpha = image->GetPixelAlpha(xx,yy);
							int newR = oldPixel.R + bright * R;
							int newG = oldPixel.G + bright * G;
							int newB = oldPixel.B + bright * B;
							if (newR > 65535) newR = 65535;
							if (newG > 65535) newG = 65535;
							if (newB > 65535) newB = 65535;

							sRGB16 pixel = { newR, newG, newB };
							int alpha = oldAlpha + bright * 65536;
							if (alpha > 65535) alpha = 65535;

							image->PutPixelImage(xx,yy,pixel);
							image->PutPixelAlpha(xx,yy,alpha);
						}
					}
				}
			}
		}
	}
}

/*
void RenderBuddhabrot(sParamRender *fractParam)
{
	isPostRendering = true;

	if (isBuddhabrot)
	{
		delete buddhabrotImg;
	}
	buddhabrotImg = new sRGB[IMAGE_WIDTH * IMAGE_HEIGHT];
	isBuddhabrot = true;

	sRGB black = { 0, 0, 0 };
	for (int i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT; i++)
	{
		buddhabrotImg[i] = black;
	}

	double last_time = clock() / CLOCKS_PER_SEC;

	CVector3 point3D1, point3D2;

	CRotationMatrix mRot;
	mRot.RotateX(-fractParam->beta);
	mRot.RotateZ(-fractParam->alfa);

	double aspectRatio = (double) IMAGE_WIDTH / IMAGE_HEIGHT;

	sFractal calcParam;

	sFractal_ret calcRet;
	calcParam.mode = normal;

	CopyParams(fractParam, &calcParam);

	guint64 buddhCounter = 0;

	int i = 0;
	while (isPostRendering)
	{
		i++;
		CVector3 point;
		point.x = fractParam->vp.x + 0.5 * fractParam->zoom * (Random(2000000) / 1000000.0 - 1.0);
		point.y = fractParam->vp.y + 0.5 * fractParam->zoom * (Random(2000000) / 1000000.0 - 1.0);
		point.z = fractParam->vp.z + 0.5 * fractParam->zoom * (Random(2000000) / 1000000.0 - 1.0);

		calcParam.point = point;
		ComputeIterations(calcParam, calcRet);

		//printf("x=%f,y=%f,z=%f, maxL=%d\n", point.x, point.y, point.z, calcRet.L);

		//if (calcRet.L < fractParam->N+10)
		//{
		for (int L = 1; L < calcRet.L; L++)
		{
			point3D1 = buddhabrot[L].point - fractParam->vp;
			//printf("B1: x=%g,y=%g,z=%g\n", point3D1.x, point3D1.y, point3D1.z);
			point3D2 = mRot.RotateVector(point3D1);
			double y2 = point3D2.y;
			double y = y2 / fractParam->zoom;
			double wsp_persp = 1.0 + y * fractParam->persp;
			double x2 = point3D2.x / wsp_persp;
			double z2 = point3D2.z / wsp_persp;
			double x = (x2 / (fractParam->zoom * aspectRatio) + 0.5) * IMAGE_WIDTH;
			double z = (z2 / fractParam->zoom + 0.5) * IMAGE_HEIGHT;
			int intX = x;
			int intY = z;
			//printf("B2: x=%g,y=%g,z=%g\n", x, y, z);
			if (intX >= 0 && intX < IMAGE_WIDTH && intY >= 0 && intY < IMAGE_HEIGHT)
			{
				if (y < complexImage[intX + intY * IMAGE_WIDTH].zBuffer)
				{
					sRGB oldPixel = buddhabrotImg[intX + intY * IMAGE_WIDTH];
					int R = oldPixel.R + calcRet.L * 2;
					if (R > 65535) R = 65535;
					int G = oldPixel.G + L * 4;
					if (G > 65535) G = 65535;
					int B = oldPixel.B + fractParam->N;
					if (B > 65535) B = 65535;
					sRGB pixel = { R, G, B };
					buddhabrotImg[intX + intY * IMAGE_WIDTH] = pixel;
					buddhCounter++;

				}
			}
			//}
		}
		double time = clock() / CLOCKS_PER_SEC;
		if (time - last_time > 5)
		{
			buddhabrotAutoBright = 10000.0 * (double) IMAGE_WIDTH * IMAGE_HEIGHT / fractParam->N / buddhCounter;
			char progressText[1000];
			last_time = clock() / CLOCKS_PER_SEC;
			double done = (double) i;
			sprintf(progressText, "Rendering Buddhabrot. Done %.3g points", done);
			gtk_progress_bar_set_text(GTK_PROGRESS_BAR(Interface.progressBar), progressText);

			CompileImage();
			Bitmap32to8(complexImage, rgbbuf, rgbbuf2);

			RedrawImage(darea, rgbbuf);

			while (gtk_events_pending())
				gtk_main_iteration();
		}
	}
}
*/
from qgis.core import (
    QgsProcessing,
    QgsProcessingAlgorithm,
    QgsProcessingParameterFeatureSource,
    QgsProcessingParameterFileDestination,
)
import os


class CoastlineToText(QgsProcessingAlgorithm):

    INPUT = "INPUT"
    OUTPUT = "OUTPUT"

    def initAlgorithm(self, config=None):
        self.addParameter(
            QgsProcessingParameterFeatureSource(
                self.INPUT,
                "Input Coastline Layer",
                [QgsProcessing.TypeVectorLine, QgsProcessing.TypeVectorPolygon]
            )
        )
        self.addParameter(
            QgsProcessingParameterFileDestination(
                self.OUTPUT,
                "Output Text File",
                fileFilter="Text files (*.txt)"
            )
        )

    def processAlgorithm(self, parameters, context, feedback):

        source = self.parameterAsSource(parameters, self.INPUT, context)
        output_file = self.parameterAsFileOutput(parameters, self.OUTPUT, context)

        with open(output_file, "w", encoding="utf-8") as f:
            
            for feature in source.getFeatures():
                geom = feature.geometry()

                if geom is None:
                    continue

                # Extract vertices depending on geometry type
                if geom.isMultipart():
                    parts = geom.asMultiPolyline() if geom.type() == 1 else geom.asMultiPolygon()
                else:
                    parts = [geom.asPolyline()] if geom.type() == 1 else [geom.asPolygon()]

                for part in parts:
                    # For polygons, asPolygon() gives list of rings, so take outer ring [0]
                    if isinstance(part[0], (list, tuple)):
                        coords = part[0]
                    else:
                        coords = part

                    for x, y in coords:
                        # Normalize longitude/latitude
                        lon = ((x + 180) % 360) - 180
                        lat = max(-90, min(90, y))
                        lon *= 1
                        lon -= 0
                        lat *= 0.998
                        lat += 0
                        f.write(f"{lon:.6f} {lat:.6f}\n")

                    # Write terminator
                    f.write("b\n")

        return {self.OUTPUT: output_file}

    def name(self):
        return "coastline_to_text"

    def displayName(self):
        return "Coastline to Text (x,y with b terminator)"

    def createInstance(self):
        return CoastlineToText()

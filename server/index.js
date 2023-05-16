const fs = require("fs");
const express = require("express");
const axios = require("axios");
const cors = require("cors");

const app = express();
const port = 8888;

app.use(express.urlencoded({ extended: true }));
app.use(express.json());
app.use(cors());

const fileName = "./resources/recording.wav";

app.get("/", cors(), async (req, res) => {
  res.status(200).send("This is the server home");
});
app.post("/uploadCommand", cors(), async (req, res) => {
  var recordingFile = fs.createWriteStream(fileName, { encoding: "utf8" });
  req.on("data", function (data) {
    recordingFile.write(data);
  });

  req.on("end", async function () {
    recordingFile.end();

    const file = fs.readFileSync(fileName);
    await axios
      .post("https://api.wit.ai/speech?v=20230506", file, {
        headers: {
          Authorization: "Bearer PRCVAZ4OSQM2ZSQUWJNU6UV5DZTQ7H2O",
          "Content-Type": "audio/wav",
        },
      })
      .then((respo) => {
        console.log("Response from wit.ai: ")
        console.log(respo.status, respo.statusText);

        const data = JSON.parse(
          "[" + respo.data.replace(/}\r\n{/g, "},{") + "]"
        ).slice(-1)[0];

        const text = data.text;
        const intent_name = data.intents[0].name;
        const intent_confidence = data.intents[0].confidence;
        const device_name = data.entities["device:device"][0].value;
        const device_confidence = data.entities["device:device"][0].confidence;
        const trait_value = data.traits["wit$on_off"][0].value;
        const trait_confidence = data.traits["wit$on_off"][0].confidence;
        console.log(
          text,
          intent_name,
          intent_confidence,
          device_name,
          device_confidence,
          trait_value,
          trait_confidence
        );
        res.status(200).json({
          text,
          intent_name,
          intent_confidence,
          device_name,
          device_confidence,
          trait_value,
          trait_confidence,
        });
      })
      .catch((err) => {
        console.log(err.response);
      });
  });
});

app.listen(port, () => {
  console.log(`Listening at http://192.168.110.103:${port}`);
});


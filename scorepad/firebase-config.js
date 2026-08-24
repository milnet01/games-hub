/* Where the shared book lives.
 *
 * Paste the block the Firebase console gives you under
 *   Project settings -> Your apps -> web app
 * over the `null` below. It looks like this:
 *
 *   window.FIREBASE_CONFIG = {
 *     apiKey: "...",
 *     authDomain: "your-project.firebaseapp.com",
 *     databaseURL: "https://your-project-default-rtdb.europe-west1.firebasedatabase.app",
 *     projectId: "your-project",
 *     appId: "1:...:web:..."
 *   };
 *
 * THIS IS NOT A SECRET, and it is safe in a public repository. The Firebase
 * apiKey identifies the project; it does not authorise anything. What actually
 * protects the data is the Security Rules set in the console — see
 * scorepad/README.md, which carries the rules this app is written against.
 *
 * `databaseURL` is the one that matters here. Without it the score book still
 * works perfectly, keeping one book on this phone alone; sharing is simply
 * offered and reported as unavailable rather than failing.
 */
window.FIREBASE_CONFIG = null;
